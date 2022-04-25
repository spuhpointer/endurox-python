
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

#define MODULE "endurox"

namespace py = pybind11;

static PyObject *EnduroxException_code(PyObject *selfPtr, void *closure)
{
    try
    {
        py::handle self(selfPtr);
        py::tuple args = self.attr("args");
        py::object code = args[1];
        code.inc_ref();
        return code.ptr();
    }
    catch (py::error_already_set &e)
    {
        py::none ret;
        ret.inc_ref();
        return ret.ptr();
    }
}

static PyGetSetDef EnduroxException_getsetters[] = {
    {const_cast<char *>("code"), EnduroxException_code, nullptr, nullptr,
     nullptr},
    {nullptr}};

static PyObject *EnduroxException_tp_str(PyObject *selfPtr)
{
    py::str ret;
    try
    {
        py::handle self(selfPtr);
        py::tuple args = self.attr("args");
        ret = py::str(args[0]);
    }
    catch (py::error_already_set &e)
    {
        ret = "";
    }

    ret.inc_ref();
    return ret.ptr();
}

static void register_exceptions(py::module &m)
{
    static PyObject *XatmiException =
        PyErr_NewException(MODULE ".XatmiException", nullptr, nullptr);
    if (XatmiException)
    {
        PyTypeObject *as_type = reinterpret_cast<PyTypeObject *>(XatmiException);
        as_type->tp_str = EnduroxException_tp_str;
        PyObject *descr = PyDescr_NewGetSet(as_type, EnduroxException_getsetters);
        auto dict = py::reinterpret_borrow<py::dict>(as_type->tp_dict);
        dict[py::handle(((PyDescrObject *)(descr))->d_name)] = py::handle(descr);

        Py_XINCREF(XatmiException);
        m.add_object("XatmiException", py::handle(XatmiException));
    }

    static PyObject *QmException =
        PyErr_NewException(MODULE ".QmException", nullptr, nullptr);
    if (QmException)
    {
        PyTypeObject *as_type = reinterpret_cast<PyTypeObject *>(QmException);
        as_type->tp_str = EnduroxException_tp_str;
        PyObject *descr = PyDescr_NewGetSet(as_type, EnduroxException_getsetters);
        auto dict = py::reinterpret_borrow<py::dict>(as_type->tp_dict);
        dict[py::handle(((PyDescrObject *)(descr))->d_name)] = py::handle(descr);

        Py_XINCREF(QmException);
        m.add_object("QmException", py::handle(QmException));
    }

    static PyObject *UbfException =
        PyErr_NewException(MODULE ".UbfException", nullptr, nullptr);
    if (UbfException)
    {
        PyTypeObject *as_type = reinterpret_cast<PyTypeObject *>(UbfException);
        as_type->tp_str = EnduroxException_tp_str;
        PyObject *descr = PyDescr_NewGetSet(as_type, EnduroxException_getsetters);
        auto dict = py::reinterpret_borrow<py::dict>(as_type->tp_dict);
        dict[py::handle(((PyDescrObject *)(descr))->d_name)] = py::handle(descr);

        Py_XINCREF(UbfException);
        m.add_object("UbfException", py::handle(UbfException));
    }

    py::register_exception_translator([](std::exception_ptr p)
                                      {
    try {
      if (p) {
        std::rethrow_exception(p);
      }
    } catch (const qm_exception &e) {
      py::tuple args(2);
      args[0] = e.what();
      args[1] = e.code();
      PyErr_SetObject(QmException, args.ptr());
    } catch (const xatmi_exception &e) {
      py::tuple args(2);
      args[0] = e.what();
      args[1] = e.code();
      PyErr_SetObject(XatmiException, args.ptr());
    } catch (const ubf_exception &e) {
      py::tuple args(2);
      args[0] = e.what();
      args[1] = e.code();
      PyErr_SetObject(UbfException, args.ptr());
    } });
}

PYBIND11_MODULE(endurox, m)
{

    register_exceptions(m);

    // Service call info object
    py::class_<pytpsvcinfo>(m, "pytpsvcinfo")
        .def_readonly("name", &pytpsvcinfo::name)
        .def_readonly("fname", &pytpsvcinfo::fname)
        .def_readonly("flags", &pytpsvcinfo::flags)
        .def_readonly("appkey", &pytpsvcinfo::appkey)
        .def_readonly("cd", &pytpsvcinfo::cd)
        .def("cltid", [](pytpsvcinfo &inf) { 
            return py::bytes(reinterpret_cast<char *>(&inf.cltid), sizeof(inf.cltid)); })
        .def_readonly("data", &pytpsvcinfo::data);

    // Poor man's namedtuple
    py::class_<pytpreply>(m, "TpReply")
        .def_readonly("rval", &pytpreply::rval)
        .def_readonly("rcode", &pytpreply::rcode)
        .def_readonly("data", &pytpreply::data)
        .def_readonly("cd", &pytpreply::cd) // Does not unpack as the use is
                                            // rare case of tpgetrply(TPGETANY)
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

    m.def(
        "tpadvertise", [](const char *svcname, const char *funcname, const py::object &func)
        { pytpadvertise(svcname, funcname, func); },
        "Routine for advertising a service name", py::arg("svcname"), py::arg("funcname"), py::arg("func"));

    m.def(
        "tpunadvertise", [](const char *svcname)
        { ndrxpy_pytpunadvertise(svcname); },
        "Unadvertise service", py::arg("tpunadvertise"));

    m.def("run", &ndrxpy_pyrun, "Run Endurox server", py::arg("server"), py::arg("args"),
          py::arg("rmname") = "NONE");

    m.def("tpadmcall", &ndrxpy_pytpadmcall, "Administers unbooted application",
          py::arg("idata"), py::arg("flags") = 0);

    m.def("tpreturn", &ndrxpy_pytpreturn, 
            R"pbdoc(
        Return from XATMI service call. Any XATMI processing after this call
        shall not be performed, i.e. shall last operation in the XATMI service
        processing.

        This function applies to XATMI servers only.
        
        For more deatils see C call *tpreturn(3)*.

        Parameters
        ----------
        rval : int
            Return value **TPSUCCESS** for success, **TPFAIL** for returning error
            **TPEXIT** for returning error and restarting the XATMI server process.
        rcode : int
            User return code. If not used, use value **0**.
        data : dict
            XATMI buffer returned from the service
        flags : int
            Or'd flags **TPSOFTTIMEOUT** for simulating **TPETIME** error to caller.
            **TPSOFTERR** return any XATMI call error, which is set in *rval* param.
	    Default value is **0**.
        )pbdoc",
          py::arg("rval"), py::arg("rcode"), py::arg("data"),
          py::arg("flags") = 0);
    m.def("tpforward", &ndrxpy_pytpforward,
          "Routine for forwarding a service request to another service routine",
          py::arg("svc"), py::arg("data"), py::arg("flags") = 0);

    m.def(
        "xaoSvcCtx",
        []()
        {
            if (xao_svc_ctx_ptr == nullptr)
            {
                throw std::runtime_error("xaoSvcCtx is null");
            }
            return reinterpret_cast<unsigned long long>(
                (*xao_svc_ctx_ptr)(nullptr));
        },
        "Returns the OCI service handle for a given XA connection");

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

    m.def("tpexport", &ndrxpy_pytpexport,
          "Converts a typed message buffer into an exportable, "
          "machine-independent string representation, that includes digital "
          "signatures and encryption seals",
          py::arg("ibuf"), py::arg("flags") = 0);
    m.def("tpimport", &ndrxpy_pytpimport,
          "Converts an exported representation back into a typed message buffer",
          py::arg("istr"), py::arg("flags") = 0);


    m.def("tpsubscribe", &ndrxpy_pytpsubscribe, "Subscribe to event (by server)",
          py::arg("eventexpr"), py::arg("filter"), py::arg("ctl"), py::arg("flags") = 0);

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

    m.def(
        "Bfldtype", [](BFLDID fieldid)
        { return Bfldtype(fieldid); },
        "Maps field identifier to field type", py::arg("fieldid"));
    m.def(
        "Bfldno", [](BFLDID fieldid)
        { return Bfldno(fieldid); },
        "Maps field identifier to field number", py::arg("fieldid"));
    m.def(
        "Bmkfldid", [](int type, BFLDID num)
        { return Bmkfldid(type, num); },
        "Makes a field identifier", py::arg("type"), py::arg("num"));

    m.def(
        "Bfname",
        [](BFLDID fieldid)
        {
            auto *name = Bfname(fieldid);
            if (name == nullptr)
            {
                throw ubf_exception(Berror);
            }
            return name;
        },
        "Maps field identifier to field name", py::arg("fieldid"));
    m.def(
        "BFLDID",
        [](const char *name)
        {
            auto id = Bfldid(const_cast<char *>(name));
            if (id == BBADFLDID)
            {
                throw ubf_exception(Berror);
            }
            return id;
        },
        "Maps field name to field identifier", py::arg("name"));

    m.def(
        "Bboolpr",
        [](const char *expression, py::object iop)
        {
            std::unique_ptr<char, decltype(&Btreefree)> guard(
                Bboolco(const_cast<char *>(expression)), &Btreefree);
            if (guard.get() == nullptr)
            {
                throw ubf_exception(Berror);
            }

            int fd = iop.attr("fileno")().cast<py::int_>();
            std::unique_ptr<FILE, decltype(&fclose)> fiop(fdopen(dup(fd), "w"),
                                                          &fclose);
            Bboolpr(guard.get(), fiop.get());
        },
        "Print Boolean expression as parsed", py::arg("expression"),
        py::arg("iop"));

    m.def(
        "Bboolev",
        [](py::object fbfr, const char *expression)
        {
            std::unique_ptr<char, decltype(&Btreefree)> guard(
                Bboolco(const_cast<char *>(expression)), &Btreefree);
            if (guard.get() == nullptr)
            {
                throw ubf_exception(Berror);
            }
            auto buf = ndrx_from_py(fbfr);
            auto rc = Bboolev(*buf.fbfr(), guard.get());
            if (rc == -1)
            {
                throw ubf_exception(Berror);
            }
            return rc == 1;
        },
        "Evaluates buffer against expression", py::arg("fbfr"),
        py::arg("expression"));

    m.def(
        "Bfloatev",
        [](py::object fbfr, const char *expression)
        {
            std::unique_ptr<char, decltype(&Btreefree)> guard(
                Bboolco(const_cast<char *>(expression)), &Btreefree);
            if (guard.get() == nullptr)
            {
                throw ubf_exception(Berror);
            }
            auto buf = ndrx_from_py(fbfr);
            auto rc = Bfloatev(*buf.fbfr(), guard.get());
            if (rc == -1)
            {
                throw ubf_exception(Berror);
            }
            return rc;
        },
        "Returns value of expression as a double", py::arg("fbfr"),
        py::arg("expression"));

    m.def(
        "Bfprint",
        [](py::object fbfr, py::object iop)
        {
            auto buf = ndrx_from_py(fbfr);
            int fd = iop.attr("fileno")().cast<py::int_>();
            std::unique_ptr<FILE, decltype(&fclose)> fiop(fdopen(dup(fd), "w"),
                                                          &fclose);
            auto rc = Bfprint(*buf.fbfr(), fiop.get());
            if (rc == -1)
            {
                throw ubf_exception(Berror);
            }
        },
        "Prints fielded buffer to specified stream", py::arg("fbfr"),
        py::arg("iop"));

    m.def(
        "Bextread",
        [](py::object iop)
        {
            xatmibuf obuf("UBF", 1024);
            int fd = iop.attr("fileno")().cast<py::int_>();
            std::unique_ptr<FILE, decltype(&fclose)> fiop(fdopen(dup(fd), "r"),
                                                          &fclose);

            obuf.mutate([&](UBFH *fbfr)
                        { return Bextread(fbfr, fiop.get()); });
            return ndrx_to_py(obuf, true);
        },
        "Builds fielded buffer from printed format", py::arg("iop"));

    //Logging functions:
    m.def(
        "tplog_debug",
        [](const char *message)
        {
            tplog(log_debug, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_info",
        [](const char *message)
        {
            tplog(log_info, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_warn",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print warning log message", py::arg("message"));

    m.def(
        "tplog_error",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print error log message", py::arg("message"));

    m.def(
        "tplog_always",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print fatal log message", py::arg("message"));

    //TODO: add pure tplog()
    
    m.def(
        "tplogconfig",
        [](int logger, int lev, const char *debug_string, const char *module, const char *new_file)
        {
            if (EXSUCCEED!=tplogconfig(logger, lev, const_cast<char *>(debug_string), 
                const_cast<char *>(module), const_cast<char *>(new_file)))
            {
                throw xatmi_exception(tperrno);   
            }
        },
        "Configure logger", py::arg("logger"), py::arg("lev"), 
        py::arg("debug_string"), py::arg("module"), py::arg("new_file"));

    m.def(
        "tplogqinfo",
        [](int lev, long flags)
        {
            long ret=tplogqinfo(lev,flags);
            if (EXFAIL==ret)
            {
                throw xatmi_exception(tperrno);   
            }

            return ret;
        },
        "Get logger info", py::arg("lev"), py::arg("flags"));

    //Event subscribtions
    m.attr("TPEVSERVICE") = py::int_(TPEVSERVICE);
    m.attr("TPEVQUEUE") = py::int_(TPEVQUEUE);//RFU
    m.attr("TPEVTRAN") = py::int_(TPEVTRAN);//RFU
    m.attr("TPEVPERSIST") = py::int_(TPEVPERSIST);//RFU

    //tplogqinfo flags:
    m.attr("TPLOGQI_GET_NDRX") = py::int_(TPLOGQI_GET_NDRX);
    m.attr("TPLOGQI_GET_UBF") = py::int_(TPLOGQI_GET_UBF);
    m.attr("TPLOGQI_GET_TP") = py::int_(TPLOGQI_GET_TP);
    m.attr("TPLOGQI_EVAL_RETURN") = py::int_(TPLOGQI_EVAL_RETURN);
    m.attr("TPLOGQI_RET_HAVDETAILED") = py::int_(TPLOGQI_RET_HAVDETAILED);

    //XATMI IPC flags:
    m.attr("TPNOFLAGS") = py::int_(TPNOFLAGS);
    m.attr("TPNOBLOCK") = py::int_(TPNOBLOCK);
    m.attr("TPSIGRSTRT") = py::int_(TPSIGRSTRT);
    m.attr("TPNOREPLY") = py::int_(TPNOREPLY);
    m.attr("TPNOTRAN") = py::int_(TPNOTRAN);
    m.attr("TPTRAN") = py::int_(TPTRAN);
    m.attr("TPNOTIME") = py::int_(TPNOTIME);
    m.attr("TPABSOLUTE") = py::int_(TPABSOLUTE);
    m.attr("TPGETANY") = py::int_(TPGETANY);
    m.attr("TPNOCHANGE") = py::int_(TPNOCHANGE);
    m.attr("TPCONV") = py::int_(TPCONV);
    m.attr("TPSENDONLY") = py::int_(TPSENDONLY);
    m.attr("TPRECVONLY") = py::int_(TPRECVONLY);

    m.attr("TPFAIL") = py::int_(TPFAIL);
    m.attr("TPSUCCESS") = py::int_(TPSUCCESS);
    m.attr("TPEXIT") = py::int_(TPEXIT);
    
    //XATMI errors:
    m.attr("TPEABORT") = py::int_(TPEABORT);
    m.attr("TPEBADDESC") = py::int_(TPEBADDESC);
    m.attr("TPEBLOCK") = py::int_(TPEBLOCK);
    m.attr("TPEINVAL") = py::int_(TPEINVAL);
    m.attr("TPELIMIT") = py::int_(TPELIMIT);
    m.attr("TPENOENT") = py::int_(TPENOENT);
    m.attr("TPEOS") = py::int_(TPEOS);
    m.attr("TPEPERM") = py::int_(TPEPERM);
    m.attr("TPEPROTO") = py::int_(TPEPROTO);
    m.attr("TPESVCERR") = py::int_(TPESVCERR);
    m.attr("TPESVCFAIL") = py::int_(TPESVCFAIL);
    m.attr("TPESYSTEM") = py::int_(TPESYSTEM);
    m.attr("TPETIME") = py::int_(TPETIME);
    m.attr("TPETRAN") = py::int_(TPETRAN);
    m.attr("TPGOTSIG") = py::int_(TPGOTSIG);
    m.attr("TPERMERR") = py::int_(TPERMERR);
    m.attr("TPEITYPE") = py::int_(TPEITYPE);
    m.attr("TPEOTYPE") = py::int_(TPEOTYPE);
    m.attr("TPERELEASE") = py::int_(TPERELEASE);
    m.attr("TPEHAZARD") = py::int_(TPEHAZARD);
    m.attr("TPEHEURISTIC") = py::int_(TPEHEURISTIC);
    m.attr("TPEEVENT") = py::int_(TPEEVENT);
    m.attr("TPEMATCH") = py::int_(TPEMATCH);
    m.attr("TPEDIAGNOSTIC") = py::int_(TPEDIAGNOSTIC);
    m.attr("TPEMIB") = py::int_(TPEMIB);
    
    //UBF errors:
    
    m.attr("BERFU0") = py::int_(BERFU0);
    m.attr("BALIGNERR") = py::int_(BALIGNERR);
    m.attr("BNOTFLD") = py::int_(BNOTFLD);
    m.attr("BNOSPACE") = py::int_(BNOSPACE);
    m.attr("BNOTPRES") = py::int_(BNOTPRES);
    m.attr("BBADFLD") = py::int_(BBADFLD);
    m.attr("BTYPERR") = py::int_(BTYPERR);
    m.attr("BEUNIX") = py::int_(BEUNIX);
    m.attr("BBADNAME") = py::int_(BBADNAME);
    m.attr("BMALLOC") = py::int_(BMALLOC);
    m.attr("BSYNTAX") = py::int_(BSYNTAX);
    m.attr("BFTOPEN") = py::int_(BFTOPEN);
    m.attr("BFTSYNTAX") = py::int_(BFTSYNTAX);
    m.attr("BEINVAL") = py::int_(BEINVAL);
    m.attr("BERFU1") = py::int_(BERFU1);
    m.attr("BBADTBL") = py::int_(BBADTBL);
    m.attr("BBADVIEW") = py::int_(BBADVIEW);
    m.attr("BVFSYNTAX") = py::int_(BVFSYNTAX);
    m.attr("BVFOPEN") = py::int_(BVFOPEN);
    m.attr("BBADACM") = py::int_(BBADACM);
    m.attr("BNOCNAME") = py::int_(BNOCNAME);
    m.attr("BEBADOP") = py::int_(BEBADOP);

    //Queue errors:
    m.attr("QMEINVAL") = py::int_(QMEINVAL);
    m.attr("QMEBADRMID") = py::int_(QMEBADRMID);
    m.attr("QMENOTOPEN") = py::int_(QMENOTOPEN);
    m.attr("QMETRAN") = py::int_(QMETRAN);
    m.attr("QMEBADMSGID") = py::int_(QMEBADMSGID);
    m.attr("QMESYSTEM") = py::int_(QMESYSTEM);
    m.attr("QMEOS") = py::int_(QMEOS);
    m.attr("QMEABORTED") = py::int_(QMEABORTED);
    m.attr("QMENOTA") = py::int_(QMENOTA);
    m.attr("QMEPROTO") = py::int_(QMEPROTO);
    m.attr("QMEBADQUEUE") = py::int_(QMEBADQUEUE);
    m.attr("QMENOMSG") = py::int_(QMENOMSG);
    m.attr("QMEINUSE") = py::int_(QMEINUSE);
    m.attr("QMENOSPACE") = py::int_(QMENOSPACE);
    m.attr("QMERELEASE") = py::int_(QMERELEASE);
    m.attr("QMEINVHANDLE") = py::int_(QMEINVHANDLE);
    m.attr("QMESHARE") = py::int_(QMESHARE);

    m.attr("BFLD_SHORT") = py::int_(BFLD_SHORT);
    m.attr("BFLD_LONG") = py::int_(BFLD_LONG);
    m.attr("BFLD_CHAR") = py::int_(BFLD_CHAR);
    m.attr("BFLD_FLOAT") = py::int_(BFLD_FLOAT);
    m.attr("BFLD_DOUBLE") = py::int_(BFLD_DOUBLE);
    m.attr("BFLD_STRING") = py::int_(BFLD_STRING);
    m.attr("BFLD_CARRAY") = py::int_(BFLD_CARRAY);
    m.attr("BFLD_UBF") = py::int_(BFLD_UBF);
    m.attr("BBADFLDID") = py::int_(BBADFLDID);

    m.attr("TPEX_STRING") = py::int_(TPEX_STRING);

    m.attr("TPMULTICONTEXTS") = py::int_(TPMULTICONTEXTS);

    m.attr("MIB_LOCAL") = py::int_(MIB_LOCAL);

    m.attr("TAOK") = py::int_(TAOK);
    m.attr("TAUPDATED") = py::int_(TAUPDATED);
    m.attr("TAPARTIAL") = py::int_(TAPARTIAL);

    m.attr("TPBLK_NEXT") = py::int_(TPBLK_NEXT);
    m.attr("TPBLK_ALL") = py::int_(TPBLK_ALL);

    m.attr("TPQCORRID") = py::int_(TPQCORRID);
    m.attr("TPQFAILUREQ") = py::int_(TPQFAILUREQ);
    m.attr("TPQBEFOREMSGID") = py::int_(TPQBEFOREMSGID);
    m.attr("TPQGETBYMSGIDOLD") = py::int_(TPQGETBYMSGIDOLD);
    m.attr("TPQMSGID") = py::int_(TPQMSGID);
    m.attr("TPQPRIORITY") = py::int_(TPQPRIORITY);
    m.attr("TPQTOP") = py::int_(TPQTOP);
    m.attr("TPQWAIT") = py::int_(TPQWAIT);
    m.attr("TPQREPLYQ") = py::int_(TPQREPLYQ);
    m.attr("TPQTIME_ABS") = py::int_(TPQTIME_ABS);
    m.attr("TPQTIME_REL") = py::int_(TPQTIME_REL);
    m.attr("TPQGETBYCORRIDOLD") = py::int_(TPQGETBYCORRIDOLD);
    m.attr("TPQPEEK") = py::int_(TPQPEEK);
    m.attr("TPQDELIVERYQOS") = py::int_(TPQDELIVERYQOS);
    m.attr("TPQREPLYQOS  ") = py::int_(TPQREPLYQOS);
    m.attr("TPQEXPTIME_ABS") = py::int_(TPQEXPTIME_ABS);
    m.attr("TPQEXPTIME_REL") = py::int_(TPQEXPTIME_REL);
    m.attr("TPQEXPTIME_NONE ") = py::int_(TPQEXPTIME_NONE);
    m.attr("TPQGETBYMSGID") = py::int_(TPQGETBYMSGID);
    m.attr("TPQGETBYCORRID") = py::int_(TPQGETBYCORRID);
    m.attr("TPQQOSDEFAULTPERSIST") = py::int_(TPQQOSDEFAULTPERSIST);
    m.attr("TPQQOSPERSISTENT ") = py::int_(TPQQOSPERSISTENT);
    m.attr("TPQQOSNONPERSISTENT") = py::int_(TPQQOSNONPERSISTENT);

    //Logger topics:

    m.attr("LOG_FACILITY_NDRX") = py::int_(LOG_FACILITY_NDRX);
    m.attr("LOG_FACILITY_UBF") = py::int_(LOG_FACILITY_UBF);
    m.attr("LOG_FACILITY_TP") = py::int_(LOG_FACILITY_TP);
    m.attr("LOG_FACILITY_TP_THREAD") = py::int_(LOG_FACILITY_TP_THREAD);
    m.attr("LOG_FACILITY_TP_REQUEST") = py::int_(LOG_FACILITY_TP_REQUEST);
    m.attr("LOG_FACILITY_NDRX_THREAD") = py::int_(LOG_FACILITY_NDRX_THREAD);
    m.attr("LOG_FACILITY_UBF_THREAD") = py::int_(LOG_FACILITY_UBF_THREAD);
    m.attr("LOG_FACILITY_NDRX_REQUEST") = py::int_(LOG_FACILITY_NDRX_REQUEST);
    m.attr("LOG_FACILITY_UBF_REQUEST") = py::int_(LOG_FACILITY_UBF_REQUEST);

    //Log levels:
    m.attr("log_always") = py::int_(log_always);
    m.attr("log_error") = py::int_(log_error);
    m.attr("log_warn") = py::int_(log_warn);
    m.attr("log_info") = py::int_(log_info);
    m.attr("log_debug") = py::int_(log_debug);
    m.attr("log_dump") = py::int_(log_dump);

    m.doc() =
        R"pbdoc(
Python3 bindings for writing Endurox clients and servers
--------------------------------------------------------

    .. currentmodule:: endurox

    .. autosummary::
        :toctree: _generate

        tpinit
        tpterm
        tpcall
        tpreturn
        tpadvertise

XATMI buffer formats
********************

Core of **XATMI** **IPC** consists of messages being sent between binaries. Message may
encode different type of data. Enduro/X supports following data buffer types:

- | **UBF** (Unified Buffer Format) which is similar to **JSON** or **YAML** buffer format, except
  | that it is typed and all fields must be defined in definition (fd) files. Basically
  | it is dictionary where every field may have several occurrences (i.e. kind of array).
  | Following field types are supported: *BFLD_CHAR* (C char type), *BFLD_SHORT* (C short type),
  | *BFLD_LONG* (C long type), *BFLD_FLOAT* (C float type), *BFLD_DOUBLE* (C double type),
  | *BFLD_STRING* (C zero terminated string type), *BFLD_CARRAY* (byte array), *BFLD_VIEW*
  | (C structure record), *BFLD_UBF* (recursive buffer) and *BFLD_PTR* (pointer to another
  | XATMI buffer).
- | **STRING** this is plain C string buffer. When using with Python, data is converted
  | from to/from *UTF-8* format.
- | **CARRAY** byte array buffer.
- | **NULL** this is empty buffer without any data and type. This buffer cannot be associated
  | with call-info buffer.
- | **JSON** this basically is C string buffer, but with indication that it contains JSON
  | formatted data. These buffer may be automatically converted to UBF and vice versa
  | for certain XATMI server configurations.
- | **VIEW** this buffer basically may hold a C structure record.

Following chapters lists XATMI data encoding principles.

UBF Data encoding
=================

When building XATMI buffer from Python dictionary, endurox-python library accepts
values to be present as list of values, in such case values are loaded into UBF occurrences
accordingly. value may be presented directly without the list, in such case the value
is loaded into UBF field occurrence **0**.

When XATMI UBF buffer dictionary is received from Enduro/X, all values are loaded into lists,
regardless of did field had several occurrences or just one.

UBF buffer type is selected by following rules:

- **data** key is dictionary and **buftype** key is not present.

- **data** key is dictionary and **buftype** key is set to **UBF**.

Example call to echo service:

.. code-block:: python
   :caption: UBF buffer encoding call
   :name: ubf-call

    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "data":{
        # 3x occs:
        "T_CHAR_FLD": ["X", "Y", 0]
        , "T_SHORT_FLD": 3200
        , "T_LONG_FLD": 99999111
        , "T_FLOAT_FLD": 1000.99
        , "T_DOUBLE_FLD": 1000111.99
        , "T_STRING_FLD": "HELLO INPUT"
        # contains sub-ubf buffer, which againt contains sub-buffer
        , "T_UBF_FLD": {"T_SHORT_FLD":99, "T_UBF_FLD":{"T_LONG_2_FLD":1000091}}
        # at occ 0 EMPTY view is used
        , "T_VIEW_FLD": [ {}, {"vname":"UBTESTVIEW2", "data":{
                        "tshort1":5
                        , "tlong1":100000
                        , "tchar1":"J"
                        , "tfloat1":9999.9
                        , "tdouble1":11119999.9
                        , "tstring1":"HELLO VIEW"
                        , "tcarray1":[b'\x00\x00', b'\x01\x01'] 
                        }}]
        # contains pointer to STRING buffer:
        , "T_PTR_FLD":{"data":"HELLO WORLD"}
    }})

    print(retbuf)


.. code-block:: python
   :caption: UBF buffer encoding output (line wrapped)
   :name: ubf-call-output
   
    {
        'buftype': 'UBF', 'data':
        {
            'T_SHORT_FLD': [3200]
            , 'T_LONG_FLD': [99999111]
            , 'T_CHAR_FLD': ['X', 'Y', b'\x00']
            , 'T_FLOAT_FLD': [1000.989990234375]
            , 'T_DOUBLE_FLD': [1000111.99]
            , 'T_STRING_FLD': ['HELLO INPUT']
            , 'T_PTR_FLD': [{'buftype': 'STRING', 'data': 'HELLO WORLD'}]
            , 'T_UBF_FLD': [{'T_SHORT_FLD': [99], 'T_UBF_FLD': [{'T_LONG_2_FLD': [1000091]}]}]
            , 'T_VIEW_FLD': [{}, {'vname': 'UBTESTVIEW2', 'data': {
                    'tshort1': [5]
                    , 'tlong1': [100000]
                    , 'tchar1': ['J']
                    , 'tfloat1': [9999.900390625]
                    , 'tdouble1': [11119999.9]
                    , 'tstring1': ['HELLO VIEW']
                    , 'tcarray1': [b'\x00\x00', b'\x01\x01']
            }}]
        }
    }

Following **exceptions** may be throw, when XATMI buffer is instantiated:

- | XatmiException with code: **TPENOENT** - view name in vname is not found. 
- | UbfException with code: **BEINVAL** - invalid view field occurrance.
  | **BNOSPACE** - no space in view field.

STRING Data encoding
====================

STRING data buffer may contain arbitrary UTF-8 string.
STRING buffer type is selected by following rules:

- **data** key value is string (does not contain 0x00 byte) and **buftype** key is not present.

- **buftype** key is set and contains **STRING** keyword.

.. code-block:: python
   :caption: STRING buffer encoding call
   :name: string-call
    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "data":"HELLO WORLD" })

    print(retbuf)

.. code-block:: python
   :caption: STRING buffer encoding output
   :name: sring-call-output

    {'buftype': 'STRING', 'data': 'HELLO WORLD'}


CARRAY Data encoding
====================

CARRAY buffer type may transport arbitrary byte array.
CARRAY buffer type is selected by following rules:

- **data** key value is byte array and **buftype** key is not present.
- **data** key value is byte array and **buftype** is set to **CARRAY**.

.. code-block:: python
   :caption: CARRAY buffer encoding call
   :name: carray-call

    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "data":b'\x00\x00\x01\x02\x04' })

    print(retbuf)

.. code-block:: python
   :caption: CARRAY buffer encoding output
   :name: carray-call-output

    {'buftype': 'CARRAY', 'data': b'\x00\x00\x01\x02\x04'}

NULL Data encoding
==================

NULL buffers are empty dictionaries, selected by following rules:

- **data** key value is empty dictionary and **buftype** key is not present.
- **data** key value is empty dictionary and **buftype** is set to **NULL**.

.. code-block:: python
   :caption: NULL buffer encoding call
   :name: null-call

    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", {})

    print(retbuf)
    
.. code-block:: python
   :caption: NULL buffer encoding output
   :name: null-call-output

    {'buftype': 'NULL'}

JSON Data encoding
==================

JSON buffer type basically is valid UTF-8 string, but with indication that
it contains json formatted data. JSON buffer is selected by following rules:

- **data** is string value and **buftype** is set to **JSON**.

.. code-block:: python
   :caption: JSON buffer encoding call
   :name: json-call

    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"JSON", "data":'{"name":"Jim", "age":30, "car":null}'})

    print(retbuf)

.. code-block:: python
   :caption: JSON buffer encoding output
   :name: json-call-output

    {'buftype': 'JSON', 'data': '{"name":"Jim", "age":30, "car":null}'}

VIEW Data encoding
==================

VIEW buffer encodes record/structure data. On the Python side data is encoded in dictionary,
and similary as with UBF, values may be set as direct values for the dictionary keys
(and are loaded into occurrence 0 of the view field). Or lists may be used to encode
values, if the view field is array, in such case values are loaded in corresponding
occurrences.

When Python code receives VIEW buffer, any NULL fields (as set by **NULL_VAL** see **viewfile(5)**)
are not converted to Python dictionary values, except in case if NULLs proceed valid array values.

For received buffers all values are encapsulated in lists.

VIEW buffer type is selected by following rules:

- **buftype** is set to **VIEW**, **subtype** is set to valid view name and **data** is dictionary.

.. code-block:: python
   :caption: VIEW buffer encoding call
   :name: view-call

    import endurox as e

    tperrno, tpurcode, retbuf = e.tpcall("ECHO", { "buftype":"VIEW", "subtype":"UBTESTVIEW2", "data":{
        "tshort1":5
        , "tlong1":100000
        , "tchar1":"J"
        , "tfloat1":9999.9
        , "tdouble1":11119999.9
        , "tstring1":"HELLO VIEW"
        , "tcarray1":[b'\x00\x00', b'\x01\x01']
    }})

    print(retbuf)

.. code-block:: python
   :caption: VIEW buffer encoding output
   :name: view-call-output

    {'buftype': 'VIEW', 'subtype': 'UBTESTVIEW2', 'data': {
        'tshort1': [5]
        , 'tlong1': [100000]
        , 'tchar1': ['J']
        , 'tfloat1': [9999.900390625]
        , 'tdouble1': [11119999.9]
        , 'tstring1': ['HELLO VIEW']
        , 'tcarray1': [b'\x00\x00', b'\x01\x01']
        }
    }

Flags to service routines
**************************

- TPNOBLOCK - non-blocking send/rcv
- TPSIGRSTRT - restart rcv on interrupt
- TPNOREPLY - no reply expected
- TPNOTRAN - not sent in transaction mode
- TPTRAN - sent in transaction mode
- TPNOTIME - no timeout
- TPABSOLUTE - absolute value on tmsetprio
- TPGETANY - get any valid reply
- TPNOCHANGE - force incoming buffer to match
- RESERVED_BIT1 - reserved for future use
- TPCONV - conversational service
- TPSENDONLY - send-only mode
- TPRECVONLY - recv-only mode

Flags to tpreturn
*****************

- TPFAIL - service FAILURE for tpreturn
- TPEXIT - service FAILURE with server exit
- TPSUCCESS - service SUCCESS for tpreturn

Flags to tpsblktime/tpgblktime
******************************

- TPBLK_SECOND - This flag sets the blocktime value, in seconds. This is default behavior.
- TPBLK_NEXT - This flag sets the blocktime value for the next potential blocking API.
- TPBLK_ALL - This flag sets the blocktime value for the all subsequent potential blocking APIs.

Flags to tpenqueue/tpdequeue
****************************

- TPQCORRID - set/get correlation id
- TPQFAILUREQ - set/get failure queue
- TPQBEFOREMSGID - enqueue before message id
- TPQGETBYMSGIDOLD - deprecated
- TPQMSGID - get msgid of enq/deq message
- TPQPRIORITY - set/get message priority
- TPQTOP - enqueue at queue top
- TPQWAIT - wait for dequeuing
- TPQREPLYQ - set/get reply queue
- TPQTIME_ABS - set absolute time
- TPQTIME_REL - set absolute time
- TPQGETBYCORRIDOLD - deprecated
- TPQPEEK - peek
- TPQDELIVERYQOS - delivery quality of service
- TPQREPLYQOS   - reply message quality of service
- TPQEXPTIME_ABS - absolute expiration time
- TPQEXPTIME_REL - relative expiration time
- TPQEXPTIME_NONE  - never expire
- TPQGETBYMSGID - dequeue by msgid
- TPQGETBYCORRID - dequeue by corrid
- TPQQOSDEFAULTPERSIST - queue's default persistence policy
- TPQQOSPERSISTENT  - disk message
- TPQQOSNONPERSISTENT - memory message

)pbdoc";
}


/* vim: set ts=4 sw=4 et smartindent: */

