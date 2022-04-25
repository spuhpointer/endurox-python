
#include <dlfcn.h>

#include <atmi.h>
#include <tpadm.h>
#include <userlog.h>
#include <xa.h>
#include <ubf.h>
#undef _

#include "exceptions.h"
#include "ndrx_pymod.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>
#include <map>

namespace py = pybind11;

/**
 * @brief export XATMI buffer
 * @param [in] idata XATMI buffer to export
 * @param [in] flags flags
 * @return exported buffer (JSON string or base64 string (if TPEX_STRING flag is set))
 */
expublic py::object ndrxpy_pytpexport(py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);
    std::vector<char> ostr;
    ostr.resize(512 + in.len * 2);

    long olen = ostr.capacity();
    int rc = tpexport(in.p, in.len, &ostr[0], &olen, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }

    if (flags == 0)
    {
        return py::bytes(&ostr[0], olen);
    }
    return py::str(&ostr[0]);
}

/**
 * @brief import XATMI buffer
 * @param [in] istr input buffer / string
 * @param [in] flags
 * @return XATMI buffer
 */
expublic py::object ndrxpy_pytpimport(const std::string istr, long flags)
{
    xatmibuf obuf("UBF", istr.size());

    long olen = 0;
    int rc = tpimport(const_cast<char *>(istr.c_str()), istr.size(), obuf.pp,
                      &olen, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }

    return ndrx_to_py(obuf, true);
}

/**
 * @brief post event 
 * @param [in] eventname name of the event
 * @param [in] data XATMI data to post
 * @param [in] flags
 * @return number of postings 
 */
expublic int ndrxpy_pytppost(const std::string eventname, py::object data, long flags)
{
    int rc=0;
    
    auto in = ndrx_from_py(data);
    {
        py::gil_scoped_release release;
        rc = tppost(const_cast<char *>(eventname.c_str()), *in.pp, in.len, flags);
        if (rc == -1)
        {
            throw xatmi_exception(tperrno);
        }
    }

    return rc;
}

/**
 * @brief Synchronous service call
 * 
 * @param svc service name
 * @param idata dictionary encoded xatmi buffer
 * @param flags any flags
 * @return pytpreply return tuple loaded with tperrno, tpurcode, return buffer
 */
expublic pytpreply ndrxpy_pytpcall(const char *svc, py::object idata, long flags)
{

    auto in = ndrx_from_py(idata);
    int tperrno_saved=0;
    xatmibuf out("NULL", (long)0);
    {
        py::gil_scoped_release release;
        int rc = tpcall(const_cast<char *>(svc), *in.pp, in.len, out.pp, &out.len,
                        flags);
        tperrno_saved=tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno_saved);
            }
        }
    }
    return pytpreply(tperrno_saved, tpurcode, ndrx_to_py(out, true));
}

/**
 * @brief enqueue message to persisten Q
 * 
 * @param [in] qspace queue space name
 * @param [in] qname queue name
 * @param [in] ctl controlstruct
 * @param [in] data XATMI object
 * @param [in] flags enqueue flags
 * @return queue control struct
 */
expublic TPQCTL ndrxpy_pytpenqueue(const char *qspace, const char *qname, TPQCTL *ctl,
                          py::object data, long flags)
{
    auto in = ndrx_from_py(data);
    {
        py::gil_scoped_release release;
        int rc = tpenqueue(const_cast<char *>(qspace), const_cast<char *>(qname),
                           ctl, *in.pp, in.len, flags);
        if (rc == -1)
        {
            if (tperrno == TPEDIAGNOSTIC)
            {
                throw qm_exception(ctl->diagnostic);
            }
            throw xatmi_exception(tperrno);
        }
    }
    return *ctl;
}

/**
 * @brief dequeue message from persistent Q
 * 
 * @param [in] qspace queue space name
 * @param [in] qname queue name
 * @param [in] ctl queue control struct
 * @param [in] flags flags
 * @return queue control struct, xatmi object
 */
expublic std::pair<TPQCTL, py::object> ndrx_pytpdequeue(const char *qspace,
                                                 const char *qname, TPQCTL *ctl,
                                                 long flags)
{
    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpdequeue(const_cast<char *>(qspace), const_cast<char *>(qname),
                           ctl, out.pp, &out.len, flags);
        if (rc == -1)
        {
            if (tperrno == TPEDIAGNOSTIC)
            {
                throw qm_exception(ctl->diagnostic);
            }
            throw xatmi_exception(tperrno);
        }
    }
    return std::make_pair(*ctl, ndrx_to_py(out, true));
}

/**
 * @brief async service call
 * @param [in] svc service name
 * @param [in] idata input XATMI buffer
 * @param [in] flags
 * @return call descriptor
 */
expublic int ndrxpy_pytpacall(const char *svc, py::object idata, long flags)
{

    auto in = ndrx_from_py(idata);

    py::gil_scoped_release release;
    int rc = tpacall(const_cast<char *>(svc), *in.pp, in.len, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }
    return rc;
}

/**
 * @brief Connect to conversational service
 * @param [in] svc service name
 * @param [in] idata input XATMI buffer
 * @param [in] flags
 * @return call descriptor
 */
static int ndrxpy_pytpconnect(const char *svc, py::object idata, long flags)
{

    auto in = ndrx_from_py(idata);

    py::gil_scoped_release release;
    int rc = tpconnect(const_cast<char *>(svc), *in.pp, in.len, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }
    return rc;
}

/**
 * @brief get reply from async call
 * @param [in] cd (optional)
 * @param [in] flags flags
 * @return call reply
 */
expublic pytpreplycd ndrxpy_pytpgetrply(int cd, long flags)
{
    int tperrno_saved=0;
    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpgetrply(&cd, out.pp, &out.len, flags);

        tperrno_saved = tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno_saved);
            }
        }
    }
    return pytpreplycd(tperrno_saved, tpurcode, ndrx_to_py(out, true), cd);
}


/**
 * @brief RFU Admin server call
 * @param [in] input buffer for call
 * @param [in] flags
 * @return standard reply
 */
expublic pytpreply ndrxpy_pytpadmcall(py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);
    int tperrno_saved=0;
    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpadmcall(*in.fbfr(), out.fbfr(), flags);
        tperrno_saved=tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno_saved);
            }
        }
    }
    return pytpreply(tperrno_saved, 0, ndrx_to_py(out, true));
}

/**
 * @brief register xatmi common methods
 * 
 * @param m Pybind11 module
 */
expublic void ndrxpy_register_xatmi(py::module &m)
{
    // Structures:
    // Poor man's namedtuple
    py::class_<pytpreply>(m, "TpReply")
        .def_readonly("rval", &pytpreply::rval)
        .def_readonly("rcode", &pytpreply::rcode)
        .def_readonly("data", &pytpreply::data)
        .def_readonly("cd", &pytpreply::cd)
        .def("__getitem__", [](const pytpreply &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.rval);
        } else if (i == 1) {
          return py::int_(s.rcode);
        } else if (i == 2) {
          return s.data;
        } else {
          throw py::index_error();
        } });

    //For tpgetrply, include cd
    py::class_<pytpreplycd>(m, "TpReplyCd")
        .def_readonly("rval", &pytpreply::rval)
        .def_readonly("rcode", &pytpreply::rcode)
        .def_readonly("data", &pytpreply::data)
        .def_readonly("cd", &pytpreply::cd)
        .def("__getitem__", [](const pytpreplycd &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.rval);
        } else if (i == 1) {
          return py::int_(s.rcode);
        } else if (i == 2) {
          return s.data;
        } else if (i == 3) {
          return py::int_(s.cd);
        } else {
          throw py::index_error();
        } });

    py::class_<TPQCTL>(m, "TPQCTL")
        .def(py::init([](long flags, long deq_time, long priority, long exp_time,
                         long urcode, long delivery_qos, long reply_qos,
                         const char *msgid, const char *corrid,
                         const char *replyqueue, const char *failurequeue)
                      {
             auto p = std::make_unique<TPQCTL>();
             memset(p.get(), 0, sizeof(TPQCTL));
             p->flags = flags;
             p->deq_time = deq_time;
             p->exp_time = exp_time;
             p->priority = priority;
             p->urcode = urcode;
             p->delivery_qos = delivery_qos;
             p->reply_qos = reply_qos;
             if (msgid != nullptr) {
               // Size limit and zero termination
               snprintf(p->msgid, sizeof(p->msgid), "%s", msgid);
             }
             if (corrid != nullptr) {
               snprintf(p->corrid, sizeof(p->corrid), "%s", corrid);
             }
             if (replyqueue != nullptr) {
               snprintf(p->replyqueue, sizeof(p->replyqueue), "%s", replyqueue);
             }
             if (failurequeue != nullptr) {
               snprintf(p->failurequeue, sizeof(p->failurequeue), "%s",
                        failurequeue);
             }
             return p; }),

             py::arg("flags") = 0, py::arg("deq_time") = 0,
             py::arg("priority") = 0, py::arg("exp_time") = 0,
             py::arg("urcode") = 0, py::arg("delivery_qos") = 0,
             py::arg("reply_qos") = 0, py::arg("msgid") = nullptr,
             py::arg("corrid") = nullptr, py::arg("replyqueue") = nullptr,
             py::arg("failurequeue") = nullptr)

        .def_readonly("flags", &TPQCTL::flags)
        .def_readonly("msgid", &TPQCTL::msgid)
        .def_readonly("diagnostic", &TPQCTL::diagnostic)
        .def_readonly("priority", &TPQCTL::priority)
        .def_readonly("corrid", &TPQCTL::corrid)
        .def_readonly("urcode", &TPQCTL::urcode)
        .def_readonly("replyqueue", &TPQCTL::replyqueue)
        .def_readonly("failurequeue", &TPQCTL::failurequeue)
        .def_readonly("delivery_qos", &TPQCTL::delivery_qos)
        .def_readonly("reply_qos", &TPQCTL::reply_qos);

    //TPEVCTL mapping
    py::class_<TPEVCTL>(m, "TPEVCTL")
        .def(py::init([](long flags, const char *name1, const char *name2)
            {
             auto p = std::make_unique<TPEVCTL>();
             memset(p.get(), 0, sizeof(TPEVCTL));
             p->flags = flags;

             if (name1 != nullptr) {
               NDRX_STRCPY_SAFE(p->name1, name1);
             }

             if (name2 != nullptr) {
               NDRX_STRCPY_SAFE(p->name2, name2);
             }

             return p; }),

             py::arg("flags") = 0, py::arg("name1") = nullptr,
             py::arg("name2") = nullptr)

        .def_readonly("flags", &TPEVCTL::flags)
        .def_readonly("name1", &TPEVCTL::name1)
        .def_readonly("name2", &TPEVCTL::name2);

    //Functions:
    m.def("tpenqueue", &ndrxpy_pytpenqueue, "Routine to enqueue a message.",
          py::arg("qspace"), py::arg("qname"), py::arg("ctl"), py::arg("data"),
          py::arg("flags") = 0);

    m.def("tpdequeue", &ndrx_pytpdequeue, "Routine to dequeue a message from a queue.",
          py::arg("qspace"), py::arg("qname"), py::arg("ctl"),
          py::arg("flags") = 0);

    m.def("tpcall", &ndrxpy_pytpcall,
          R"pbdoc(
        Synchronous service call. In case if service returns **TPFAIL** or **TPEXIT**,
        exception is not thrown, instead first return argument shall be tested for
        the tperrno for 0 (to check success case).
        
        For more deatils see **tpcall(3)**.

        :raise XatmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments to function.
            | **TPEOTYPE** - Output type not allowed.
            | **TPENOENT** - Service not advertised.
            | **TPETIME** - Service timeout.
            | **TPESVCFAIL** - Service returned **TPFAIL** or **TPEXIT** (not thrown).
            | **TPESVCERR** - Service failure during processing.
            | **TPESYSTEM** - System error.
            | **TPEOS** - System error.
            | **TPEBLOCK** - Blocking condition found and **TPNOBLOCK** flag was specified
            | **TPETRAN** - Target service is transactional, but failed to start the transaction.
            | **TPEITYPE** - Service error during input buffer handling.

        Parameters
        ----------
        svc : str
            Service name to call
        idata : dict
            Input XATMI data buffer
        flags : int
            Or'd bit flags: **TPNOTRAN**, **TPSIGRSTRT**, **TPNOTIME**, 
            **TPNOCHANGE**, **TPTRANSUSPEND**, **TPNOBLOCK**, **TPNOABORT**.

        Returns
        -------
        int
            tperrno - error code
        int
            tpurcode - code passed to **tpreturn(3)** by the server
        dict
            XATMI buffer returned from the server.

     )pbdoc",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpacall", &ndrxpy_pytpacall, "Routine for sending a service request",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpgetrply", &ndrxpy_pytpgetrply,
          "Routine for getting a reply from a previous request", py::arg("cd"),
          py::arg("flags") = 0);

    m.def("tpconnect", &ndrxpy_pytpconnect, "Connect to service (conversational)",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpexport", &ndrxpy_pytpexport,
          "Converts a typed message buffer into an exportable, "
          "machine-independent string representation, that includes digital "
          "signatures and encryption seals",
          py::arg("ibuf"), py::arg("flags") = 0);

    m.def("tpimport", &ndrxpy_pytpimport,
          "Converts an exported representation back into a typed message buffer",
          py::arg("istr"), py::arg("flags") = 0);

    m.def("tppost", &ndrxpy_pytppost, "Posts an event", py::arg("eventname"),
          py::arg("data"), py::arg("flags") = 0);

    m.def(
        "tpgblktime",
        [](long flags)
        {
            int rc = tpgblktime(flags);
            if (rc == -1)
            {
                throw xatmi_exception(tperrno);
            }
            return rc;
        },
        "Retrieves a previously set, per second or millisecond, blocktime value",
        py::arg("flags"));

    m.def(
        "tpsblktime",
        [](int blktime, long flags)
        {
            if (tpsblktime(blktime, flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Routine for setting blocktime in seconds or milliseconds for the next "
        "service call or for all service calls",
        py::arg("blktime"), py::arg("flags"));


    m.def("tpadmcall", &ndrxpy_pytpadmcall, "Administers unbooted application",
          py::arg("idata"), py::arg("flags") = 0);

    m.def(
        "tpinit",
        [](long flags)
        {
            py::gil_scoped_release release;

            TPINIT init;
            memset(&init, 0, sizeof(init));

            init.flags = flags;

            if (tpinit(&init) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Joins thread to application
        
        For more deatils see C call *tpinit(3)*.

        :raise XatmiException: 
            | Following error codes may be present:
            | *TPEINVAL* - Unconfigured application,
            | *TPESYSTEM* - Enduro/X System error occurred,
            | *TPEOS* - Operating system error occurred.

        Parameters
        ----------
        rval : int
            | Or'd flags, default is 0: 
            | **TPU_IGN** - ignore incoming unsolicited messages.
     )pbdoc", py::arg("flags") = 0);

    m.def(
        "tpterm",
        []()
        {
            py::gil_scoped_release release;

            if (tpterm() == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Leaves application, closes XATMI session.
        
        For more deatils see C call *tpterm(3)*.

        :raise XatmiException: 
            | Following error codes may be present:
            | *TPEPROTO* - Called from XATMI server (main thread),
            | *TPESYSTEM* - Enduro/X System error occurred,
            | *TPEOS* - Operating system error occurred.

     )pbdoc");

    m.def(
        "tpbegin",
        [](unsigned long timeout, long flags)
        {
            py::gil_scoped_release release;
            if (tpbegin(timeout, flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Routine for beginning a transaction", py::arg("timeout"),
        py::arg("flags") = 0);

    m.def(
        "tpsuspend",
        [](long flags)
        {
            TPTRANID tranid;
            py::gil_scoped_release release;
            if (tpsuspend(&tranid, flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
            return py::bytes(reinterpret_cast<char *>(&tranid), sizeof(tranid));
        },
        "Suspend a global transaction", py::arg("flags") = 0);

    m.def(
        "tpresume",
        [](py::bytes tranid, long flags)
        {
            py::gil_scoped_release release;
            if (tpresume(reinterpret_cast<TPTRANID *>(
#if PY_MAJOR_VERSION >= 3
                             PyBytes_AsString(tranid.ptr())
#else
                             PyString_AsString(tranid.ptr())
#endif
                                 ),
                         flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Resume a global transaction", py::arg("tranid"), py::arg("flags") = 0);

    m.def(
        "tpcommit",
        [](long flags)
        {
            py::gil_scoped_release release;
            if (tpcommit(flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Routine for committing current transaction", py::arg("flags") = 0);

    m.def(
        "tpabort",
        [](long flags)
        {
            py::gil_scoped_release release;
            if (tpabort(flags) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Routine for aborting current transaction", py::arg("flags") = 0);

    m.def(
        "tpgetlev",
        []()
        {
            int rc;
            if ((rc = tpgetlev()) == -1)
            {
                throw xatmi_exception(tperrno);
            }
            return py::bool_(rc);
        },
        "Routine for checking if a transaction is in progress");

    m.def(
        "tpopen",
        []()
        {
            int rc;
            if ((rc = tpopen()) == -1)
            {
                throw xatmi_exception(tperrno);
            }
            return py::bool_(rc);
        },
        "Open XA Sub-system");
    m.def(
        "tpclose",
        []()
        {
            int rc;
            if ((rc = tpclose()) == -1)
            {
                throw xatmi_exception(tperrno);
            }
            return py::bool_(rc);
        },
        "Close XA Sub-system");
    m.def(
        "userlog",
        [](const char *message)
        {
            py::gil_scoped_release release;
            userlog(const_cast<char *>("%s"), message);
        },
        "Writes a message to the Endurox ATMI system central event log",
        py::arg("message"));

}


/* vim: set ts=4 sw=4 et smartindent: */
