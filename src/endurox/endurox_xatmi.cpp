/**
 * @brief Enduro/X Python module - xatmi client/server common
 *
 * @file endurox_xatmi.cpp
 */
/* -----------------------------------------------------------------------------
 * Python module for Enduro/X
 * This software is released under MIT license.
 * 
 * -----------------------------------------------------------------------------
 * MIT License
 * Copyright (C) 2019 Aivars Kalvans <aivars.kalvans@gmail.com> 
 * Copyright (C) 2022 Mavimax SIA
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * -----------------------------------------------------------------------------
 */

#include <dlfcn.h>

#include <atmi.h>
#include <tpadm.h>
#include <userlog.h>
#include <xa.h>
#include <ubf.h>
#include <tmenv.h>
#undef _

#include "exceptions.h"
#include "ndrx_pymod.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>
#include <map>

namespace py = pybind11;


//TODO: Keep the Python func in ndrx_ctx_priv_get()

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

    return ndrx_to_py(obuf);
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
    return pytpreply(tperrno_saved, tpurcode, ndrx_to_py(out));
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
expublic NDRXPY_TPQCTL ndrxpy_pytpenqueue(const char *qspace, const char *qname, NDRXPY_TPQCTL *ctl,
                          py::object data, long flags)
{
    auto in = ndrx_from_py(data);
    {
        ctl->convert_to_base();
        TPQCTL *ctl_c = dynamic_cast<TPQCTL*>(ctl);

        py::gil_scoped_release release;

        int rc = tpenqueue(const_cast<char *>(qspace), const_cast<char *>(qname),
                           ctl_c, *in.pp, in.len, flags);
        if (rc == -1)
        {
            if (tperrno == TPEDIAGNOSTIC)
            {
                throw qm_exception(ctl->diagnostic);
            }
            throw xatmi_exception(tperrno);
        }
    }

    ctl->convert_from_base();

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
expublic std::pair<NDRXPY_TPQCTL, py::object> ndrx_pytpdequeue(const char *qspace,
                                                 const char *qname, NDRXPY_TPQCTL *ctl,
                                                 long flags)
{
    xatmibuf out("UBF", 1024);
    {
        ctl->convert_to_base();
        TPQCTL *ctl_c = dynamic_cast<TPQCTL*>(ctl);

        py::gil_scoped_release release;
        int rc = tpdequeue(const_cast<char *>(qspace), const_cast<char *>(qname),
                           ctl_c, out.pp, &out.len, flags);
        if (rc == -1)
        {
            if (tperrno == TPEDIAGNOSTIC)
            {
                throw qm_exception(ctl->diagnostic, ctl->diagmsg);
            }
            throw xatmi_exception(tperrno);
        }
    }

    ctl->convert_from_base();
    
    return std::make_pair(*ctl, ndrx_to_py(out));
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
 * @brief Send notification to the client process
 * 
 * @param clientid client id as received by service in cltid argument
 * @param idata data to send to the client
 * @param flags 
 */
exprivate void ndrxpy_pytpnotify(py::bytes clientid, py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);

    int size = PyBytes_Size(clientid.ptr());

    //Check the size
    if (sizeof(CLIENTID)!=size)
    {
        NDRX_LOG(log_error, "Invalid `clientid': CLIENTID size is %d bytes, got %d bytes",
            sizeof(clientid), size);
        throw std::invalid_argument("INvalid `clientid' size");
    }

    CLIENTID *cltid = reinterpret_cast<CLIENTID*>(PyBytes_AsString(clientid.ptr()));

    py::gil_scoped_release release;
    //int rc = tpnotify(cltid, *in.pp, in.len, flags);
    int rc = tpnotify(cltid, *in.pp, in.len, flags);
    
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }
}

/**
 * @brief broadcast the message to the nodes
 * 
 * @param lmid Machine Ids to which send msg
 * @param usrname RFU
 * @param cltname client exe name to match the msg
 * @param idata data buffer
 * @param flags 
 */
exprivate void ndrxpy_pytpbroadcast(const char *lmid, const char *usrname, const char *cltname, 
    py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);

    py::gil_scoped_release release;
    int rc = tpbroadcast(const_cast<char *>(lmid), const_cast<char *>(usrname), 
        const_cast<char *>(cltname), *in.pp, in.len, flags);
    
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }
}

/**
 * @brief Dispatch notification 
 * 
 * @param data 
 * @param len 
 * @param flags 
 */
exprivate void notification_callback (char *data, long len, long flags)
{
    xatmibuf b;
    b.len = len;
    b.p=nullptr;//do not free the master buffer.
    b.pp = &data;

    ndrx_ctx_priv_t* priv = ndrx_ctx_priv_get();
    ndrxpy_object_t *obj_ptr = reinterpret_cast<ndrxpy_object_t *>(priv->integptr1);
    py::gil_scoped_acquire gil;

    obj_ptr->obj(ndrx_to_py(b));
}

/**
 * Set unsol handler
 */
exprivate void ndrxpy_pytpsetunsol(const py::object &func)
{
    tpsetunsol(notification_callback);

    ndrxpy_object_t *obj_ptr = new ndrxpy_object_t();
    obj_ptr->obj = func;

    ndrx_ctx_priv_t* priv = ndrx_ctx_priv_get();

    if (nullptr!=priv->integptr1)
    {
        delete reinterpret_cast<ndrxpy_object_t*>(priv->integptr1);
    }

    priv->integptr1 = reinterpret_cast<void*>(obj_ptr);
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
 * @brief Send data to conversational end-point
 * 
 * @param cd call descriptor
 * @param idata data to send
 * @param flags  flags
 * @return tperrno, revent 
 */
expublic pytpsendret ndrxpy_pytpsend(int cd, py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);
    long revent;

    int tperrno_saved;
    {
        py::gil_scoped_release release;
        int rc = tpsend(cd, *in.pp, in.len, flags, &revent);
        tperrno_saved = tperrno;

        if (rc == -1)
        {
            if (TPEEVENT!=tperrno_saved)
            {
                throw xatmi_exception(tperrno_saved);
            }
        }
    }

    return pytpsendret(tperrno_saved, revent);
}

/**
 * @brief Receive message from conversational end-point
 * 
 * @param cd call descriptor
 * @param flags flags
 * @return tperrno, revent, tpurcode, XATMI buffer
 */
expublic pytprecvret ndrxpy_pytprecv(int cd, long flags)
{
    long revent;
    int tperrno_saved;

    xatmibuf out("NULL", 0L);
    {
        py::gil_scoped_release release;
        int rc = tprecv(cd, out.pp, &out.len, flags, &revent);
        tperrno_saved = tperrno;

        if (rc == -1)
        {
            if (TPEEVENT!=tperrno_saved)
            {
                throw xatmi_exception(tperrno_saved);
            }
        }
    }

    return pytprecvret(tperrno_saved, revent, tpurcode, ndrx_to_py(out));
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
    return pytpreplycd(tperrno_saved, tpurcode, ndrx_to_py(out), cd);
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
    return pytpreply(tperrno_saved, 0, ndrx_to_py(out));
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

    //Return value for tpsend
    py::class_<pytpsendret>(m, "TpSendRet")
        .def_readonly("rval", &pytpsendret::rval)
        .def_readonly("revent", &pytpsendret::revent)
        .def("__getitem__", [](const pytpsendret &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.rval);
        } else if (i == 1) {
          return py::int_(s.revent);
        } else {
          throw py::index_error();
        } });

    //Return value for tprecv()
    py::class_<pytprecvret>(m, "TpRecvRet")
        .def_readonly("rval", &pytprecvret::rval)
        .def_readonly("revent", &pytprecvret::revent)
        .def_readonly("rcode", &pytprecvret::rcode)
        .def_readonly("data", &pytprecvret::data)
        .def("__getitem__", [](const pytprecvret &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.rval);
        } else if (i == 1) {
          return py::int_(s.revent);
        } else if (i == 2) {
            return py::int_(s.rcode);
        } else if (i == 3) {
          return s.data;
        } else {
          throw py::index_error();
        } });

    py::class_<NDRXPY_TPQCTL>(m, "TPQCTL")
        .def(py::init([](long flags, long deq_time, long priority, long exp_time,
                         long urcode, long delivery_qos, long reply_qos,
                         pybind11::bytes msgid, pybind11::bytes corrid,
                         std::string & replyqueue, std::string & failurequeue)
                      {
             auto p = std::make_unique<NDRXPY_TPQCTL>();
             //Default construction shall have performed memset
             //memset(p.get(), 0, sizeof(NDRXPY_TPQCTL));
             p->flags = flags;
             p->deq_time = deq_time;
             p->exp_time = exp_time;
             p->priority = priority;
             p->urcode = urcode;
             p->delivery_qos = delivery_qos;
             p->reply_qos = reply_qos;
             
             p->msgid = msgid;
             p->corrid = corrid;

             p->replyqueue = replyqueue;
             p->failurequeue = failurequeue;

             return p; }),

             py::arg("flags") = 0, py::arg("deq_time") = 0,
             py::arg("priority") = 0, py::arg("exp_time") = 0,
             py::arg("urcode") = 0, py::arg("delivery_qos") = 0,
             py::arg("reply_qos") = 0, py::arg("msgid") = pybind11::bytes(),
             py::arg("corrid") = pybind11::bytes(), py::arg("replyqueue") = std::string(""),
             py::arg("failurequeue") = std::string(""))

        .def_readwrite("flags", &NDRXPY_TPQCTL::flags)
        .def_readwrite("msgid", &NDRXPY_TPQCTL::msgid)
        .def_readonly("diagnostic", &NDRXPY_TPQCTL::diagnostic)
        .def_readonly("diagmsg", &NDRXPY_TPQCTL::diagmsg)
        .def_readwrite("priority", &NDRXPY_TPQCTL::priority)
        .def_readwrite("corrid", &NDRXPY_TPQCTL::corrid)
        .def_readonly("urcode", &NDRXPY_TPQCTL::urcode)
        .def_readonly("cltid", &NDRXPY_TPQCTL::cltid)
        .def_readwrite("replyqueue", &NDRXPY_TPQCTL::replyqueue)
        .def_readwrite("failurequeue", &NDRXPY_TPQCTL::failurequeue)
        .def_readwrite("delivery_qos", &NDRXPY_TPQCTL::delivery_qos)
        .def_readwrite("reply_qos", &NDRXPY_TPQCTL::reply_qos);

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

    //Context handle
    py::class_<pytpcontext>(m, "TPCONTEXT_T")
        //this is buffer for pointer...
        .def_readonly("ctx_bytes", &pytpcontext::ctx_bytes);

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

    m.def(
    "tpcancel",
    [](int cd)
    {
        py::gil_scoped_release release;

        if (tpcancel(cd) == EXFAIL)
        {
            throw xatmi_exception(tperrno);
        }
    },
    "Cancel call", py::arg("cd") = 0);

    m.def("tpconnect", &ndrxpy_pytpconnect, "Connect to service (conversational)",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    //Conversational APIs
    m.def("tpsend", &ndrxpy_pytpsend, "Send conversational data",
          py::arg("cd"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tprecv", &ndrxpy_pytprecv, "Receive conversational data",
          py::arg("cd"), py::arg("flags") = 0);

    m.def(
    "tpdiscon",
    [](int cd)
    {
        py::gil_scoped_release release;

        if (tpdiscon(cd) == EXFAIL)
        {
            throw xatmi_exception(tperrno);
        }
    },
    "Forced disconnect from conversation", py::arg("cd") = 0);
    

    //TODO: notification API.
    m.def("tpnotify", &ndrxpy_pytpnotify, "Send unsolicited notification to the process",
          py::arg("clientid"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpbroadcast", &ndrxpy_pytpbroadcast, "Broadcast unsolicited notifications to the cluster",
          py::arg("lmid"), py::arg("usrname"), py::arg("cltname"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpsetunsol", [](const py::object &func) { ndrxpy_pytpsetunsol(func); }, "Set unsolicted message callback",
          py::arg("func"));
    m.def(
        "tpchkunsol",
        []()
        {
            py::gil_scoped_release release;

            int ret = tpchkunsol();
            if (EXFAIL==ret)
            {
                throw xatmi_exception(tperrno);
            }
            return ret;
        }, "Check and process unsolicited notifications");

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

    m.def(
        "tpencrypt",
        [](py::bytes input, long flags)
        {
            if (flags & TPEX_STRING)
            {
                throw std::invalid_argument("TPEX_STRING flag may not be used in bytes input mode");
            }

            std::string val(PyBytes_AsString(input.ptr()), PyBytes_Size(input.ptr()));
            /* get the twice the output buffer... */
            tempbuf tmp(val.size() + 20 );

            {
                py::gil_scoped_release release;
            
            
                if (EXSUCCEED!=tpencrypt(const_cast<char *>(val.data()),
                                    val.size(), tmp.buf, &tmp.size, flags))
                {
                    throw xatmi_exception(tperrno);
                }
            }

            return py::bytes(tmp.buf, tmp.size);
        },
        "Encrypt data block",
        py::arg("input"), py::arg("flags")=0);

    m.def(
        "tpencrypt",
        [](py::str input, long flags)
        {
            py::bytes b = py::reinterpret_steal<py::bytes>(
                PyUnicode_EncodeLocale(input.ptr(), "surrogateescape"));

            /* get the twice the output buffer... */

            std::string val = "";
            char *ptr_val =NULL;
            long len;
            val.assign(PyBytes_AsString(b.ptr()), PyBytes_Size(b.ptr()));
            ptr_val = const_cast<char *>(val.data());
            len = val.size();

            tempbuf tmp(((len + 20 +2)/3)*4 + 1);

            {
                py::gil_scoped_release release;
            
                if (EXSUCCEED!=tpencrypt(ptr_val, len, tmp.buf, &tmp.size, flags|TPEX_STRING))
                {
                    throw xatmi_exception(tperrno);
                }
            }

            return py::str(tmp.buf);
        },
        "Encrypt data block",
        py::arg("input"), py::arg("flags")=0);

    m.def(
        "tpdecrypt",
        [](py::bytes input, long flags)
        {
            if (flags & TPEX_STRING)
            {
                throw std::invalid_argument("TPEX_STRING flag may not be used in bytes input mode");
            }

            std::string val(PyBytes_AsString(input.ptr()), PyBytes_Size(input.ptr()));

            /* get the twice the output buffer... 
             * should be larger than encrypte
             */
            tempbuf tmp(val.size()+1);
            {
                py::gil_scoped_release release;
            
                if (EXSUCCEED!=tpdecrypt(const_cast<char *>(val.data()),
                                    val.size(), tmp.buf, &tmp.size, flags))
                {
                    throw xatmi_exception(tperrno);
                }
            }
            return py::bytes(tmp.buf, tmp.size);
        },
        "Encrypt data block",
        py::arg("input"), py::arg("flags")=0);

    m.def(
        "tpdecrypt",
        [](py::str input, long flags)
        {
            py::bytes b = py::reinterpret_steal<py::bytes>(
                PyUnicode_EncodeLocale(input.ptr(), "surrogateescape"));

            std::string val = "";
            char *ptr_val =NULL;
            long len;
            val.assign(PyBytes_AsString(b.ptr()), PyBytes_Size(b.ptr()));
            ptr_val = const_cast<char *>(val.data());
            len = val.size();

            tempbuf tmp(len+1);

            {
                py::gil_scoped_release release;
            
                if (EXSUCCEED!=tpdecrypt(ptr_val, len, tmp.buf, &tmp.size, flags|TPEX_STRING))
                {
                    throw xatmi_exception(tperrno);
                }
            }

            return py::str(tmp.buf);
        },
        "Decrypt data block",
        py::arg("input"), py::arg("flags")=0);

    m.def(
        "tuxgetenv",
        [](std::string envname)
        {
            return py::str(tuxgetenv(const_cast<char *>(envname.c_str())));
        },
        "Get environment value",
        py::arg("envname"));

    m.def(
        "tpnewctxt",
        [](bool auto_destroy, bool auto_set)
        {
            auto ctxt = tpnewctxt(auto_destroy, auto_set);
            return pytpcontext(&ctxt);
        },
        "Create new ATMI context",
        py::arg("auto_destroy"), py::arg("auto_set"));

    m.def(
        "tpgetctxt",
        [](long flags)
        {
            TPCONTEXT_T ctxt;
            if (EXFAIL==tpgetctxt(&ctxt, flags))
            {
                throw xatmi_exception(tperrno);
            }

            return pytpcontext(&ctxt);
        },
        "Disassociate from the thread and get current context",
        py::arg("flags")=0);

    m.def(
        "tpsetctxt",
        [](pytpcontext *context, long flags)
        {
            TPCONTEXT_T ctxt;
            context->getCtxt(&ctxt);
            if (EXSUCCEED!=tpsetctxt(ctxt, flags))
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Set current context",
        py::arg("context"), py::arg("flags")=0);

    m.def(
        "tpsetctxt",
        [](py::none, long flags)
        {
            if (EXSUCCEED!=tpsetctxt(TPNULLCONTEXT, flags))
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Set current context to TPNULLCONTEXT",
        py::arg("context"), py::arg("flags")=0);

    m.def(
        "tpfreectxt",
        [](pytpcontext *context)
        {
            TPCONTEXT_T ctxt;
            context->getCtxt(&ctxt);

            tpfreectxt(ctxt);
        },
        "Free current context",
        py::arg("context"));

    }

/* vim: set ts=4 sw=4 et smartindent: */
