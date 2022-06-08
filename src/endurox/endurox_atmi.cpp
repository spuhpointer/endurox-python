/**
 * @brief Enduro/X Python module - atmi client/server common
 *
 * @file endurox_atmi.cpp
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

/*---------------------------Includes-----------------------------------*/

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

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/

/**
 * tpgetconn() struct values
 * this is copy+paste from unexported header from xadrv/oracle/oracle_common.c (Enduro/X source)
 */
typedef struct
{
    unsigned int magic; /**< magic number of the record                    */
    long version;       /**< record version                                */
    void *xaoSvcCtx;    /**< xaoSvcCtx handle,                             */
    
} ndrx_ora_tpgetconn_t;

/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/

namespace py = pybind11;

/**
 * @brief export ATMI buffer
 * @param [in] idata ATMI buffer to export
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
        throw atmi_exception(tperrno);
    }

    if (flags == 0)
    {
        return py::bytes(&ostr[0], olen-1);
    }
    return py::str(&ostr[0]);
}

/**
 * @brief import ATMI buffer
 * @param [in] istr input buffer / string
 * @param [in] flags
 * @return ATMI buffer
 */
expublic py::object ndrxpy_pytpimport(const std::string istr, long flags)
{
    atmibuf obuf("UBF", istr.size());

    long olen = 0;
    int rc = tpimport(const_cast<char *>(istr.c_str()), istr.size(), obuf.pp,
                      &olen, flags);
    if (rc == -1)
    {
        throw atmi_exception(tperrno);
    }

    return ndrx_to_py(obuf);
}

/**
 * @brief post event 
 * @param [in] eventname name of the event
 * @param [in] data ATMI data to post
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
            throw atmi_exception(tperrno);
        }
    }

    return rc;
}

/**
 * @brief Synchronous service call
 * 
 * @param svc service name
 * @param idata dictionary encoded atmi buffer
 * @param flags any flags
 * @return pytpreply return tuple loaded with tperrno, tpurcode, return buffer
 */
expublic pytpreply ndrxpy_pytpcall(const char *svc, py::object idata, long flags)
{

    auto in = ndrx_from_py(idata);
    int tperrno_saved=0;
    atmibuf out("NULL", (long)0);
    {
        py::gil_scoped_release release;
        int rc = tpcall(const_cast<char *>(svc), *in.pp, in.len, out.pp, &out.len,
                        flags);
        tperrno_saved=tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw atmi_exception(tperrno_saved);
            }
        }
    }
    return pytpreply(tperrno_saved, tpurcode, ndrx_to_py(out));
}

/**
 * @brief enqueue message to persistent Q
 * 
 * @param [in] qspace queue space name
 * @param [in] qname queue name
 * @param [in] ctl controlstruct
 * @param [in] data ATMI object
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
            throw atmi_exception(tperrno);
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
 * @return queue control struct, atmi object
 */
expublic std::pair<NDRXPY_TPQCTL, py::object> ndrx_pytpdequeue(const char *qspace,
                                                 const char *qname, NDRXPY_TPQCTL *ctl,
                                                 long flags)
{
    atmibuf out("UBF", 1024);
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
            throw atmi_exception(tperrno);
        }
    }

    ctl->convert_from_base();
    
    return std::make_pair(*ctl, ndrx_to_py(out));
}

/**
 * @brief async service call
 * @param [in] svc service name
 * @param [in] idata input ATMI buffer
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
        throw atmi_exception(tperrno);
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
exprivate void ndrxpy_pytpnotify(pyclientid *clientid, py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);

    int size = PyBytes_Size(clientid->pycltid.ptr());

    //Check the size
    if (sizeof(CLIENTID)!=size)
    {
        NDRX_LOG(log_error, "Invalid `clientid': CLIENTID size is %d bytes, got %d bytes",
            sizeof(clientid), size);
        throw std::invalid_argument("INvalid `clientid' size");
    }

    CLIENTID *cltid = reinterpret_cast<CLIENTID*>(PyBytes_AsString(clientid->pycltid.ptr()));

    py::gil_scoped_release release;
    //int rc = tpnotify(cltid, *in.pp, in.len, flags);
    int rc = tpnotify(cltid, *in.pp, in.len, flags);
    
    if (rc == -1)
    {
        throw atmi_exception(tperrno);
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
        throw atmi_exception(tperrno);
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
    atmibuf b;
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
    if (TPUNSOLERR==tpsetunsol(notification_callback))
    {
        throw atmi_exception(tperrno);
    }

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
 * @param [in] idata input ATMI buffer
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
        throw atmi_exception(tperrno);
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
                throw atmi_exception(tperrno_saved);
            }
        }
    }

    return pytpsendret(tperrno_saved, tpurcode, revent);
}

/**
 * @brief Receive message from conversational end-point
 * 
 * @param cd call descriptor
 * @param flags flags
 * @return tperrno, revent, tpurcode, ATMI buffer
 */
expublic pytprecvret ndrxpy_pytprecv(int cd, long flags)
{
    long revent;
    int tperrno_saved;

    atmibuf out("NULL", 0L);
    {
        py::gil_scoped_release release;
        int rc = tprecv(cd, out.pp, &out.len, flags, &revent);
        tperrno_saved = tperrno;

        if (rc == -1)
        {
            if (TPEEVENT!=tperrno_saved)
            {
                throw atmi_exception(tperrno_saved);
            }
        }
    }

    return pytprecvret(tperrno_saved, tpurcode, revent, ndrx_to_py(out));
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
    atmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpgetrply(&cd, out.pp, &out.len, flags);

        tperrno_saved = tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw atmi_exception(tperrno_saved);
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
    atmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpadmcall(*in.fbfr(), out.fbfr(), flags);
        tperrno_saved=tperrno;
        if (rc == -1)
        {
            if (tperrno_saved != TPESVCFAIL)
            {
                throw atmi_exception(tperrno_saved);
            }
        }
    }
    return pytpreply(tperrno_saved, 0, ndrx_to_py(out));
}

/**
 * @brief register atmi common methods
 * 
 * @param m Pybind11 module
 */
expublic void ndrxpy_register_atmi(py::module &m)
{
    // Structures:
    py::class_<pytptranid>(m, "TPTRANID");
    // Poor man's namedtuple
    py::class_<pytpreply>(m, "TpReply")
        .def_readonly("tperrno", &pytpreply::pytperrno)
        .def_readonly("tpurcode", &pytpreply::pytpurcode)
        .def_readonly("data", &pytpreply::data)
        .def_readonly("cd", &pytpreply::cd)
        .def("__getitem__", [](const pytpreply &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.pytperrno);
        } else if (i == 1) {
          return py::int_(s.pytpurcode);
        } else if (i == 2) {
          return s.data;
        } else {
          throw py::index_error();
        } });

    //For tpgetrply, include cd
    py::class_<pytpreplycd>(m, "TpReplyCd")
        .def_readonly("tperrno", &pytpreply::pytperrno)
        .def_readonly("tpurcode", &pytpreply::pytpurcode)
        .def_readonly("data", &pytpreply::data)
        .def_readonly("cd", &pytpreply::cd)
        .def("__getitem__", [](const pytpreplycd &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.pytperrno);
        } else if (i == 1) {
          return py::int_(s.pytpurcode);
        } else if (i == 2) {
          return s.data;
        } else if (i == 3) {
          return py::int_(s.cd);
        } else {
          throw py::index_error();
        } });

    //Return value for tpsend
    py::class_<pytpsendret>(m, "TpSendRet")
        .def_readonly("tperrno", &pytpsendret::pytperrno)
        .def_readonly("tpurcode", &pytpsendret::pytpurcode)
        .def_readonly("revent", &pytpsendret::revent)
        .def("__getitem__", [](const pytpsendret &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.pytperrno);
        } else if (i == 1) {
          return py::int_(s.pytpurcode);
        } else if (i == 2) {
          return py::int_(s.revent);
        } else {
          throw py::index_error();
        } });

    //Return value for tprecv()
    py::class_<pytprecvret>(m, "TpRecvRet")
        .def_readonly("tperrno", &pytprecvret::pytperrno)
        .def_readonly("tpurcode", &pytprecvret::pytpurcode)
        .def_readonly("revent", &pytprecvret::revent)
        .def_readonly("data", &pytprecvret::data)
        .def("__getitem__", [](const pytprecvret &s, size_t i) -> py::object
             {
        if (i == 0) {
          return py::int_(s.pytperrno);
        } else if (i == 1) {
          return py::int_(s.pytpurcode);
        } else if (i == 2) {
            return py::int_(s.revent);
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
             //auto p = std::make_unique<NDRXPY_TPQCTL>();
             auto p = std::unique_ptr<NDRXPY_TPQCTL>(new NDRXPY_TPQCTL());
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
        .def_readwrite("deq_time", &NDRXPY_TPQCTL::deq_time)
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
        .def_readwrite("reply_qos", &NDRXPY_TPQCTL::reply_qos)
        .def_readwrite("exp_time", &NDRXPY_TPQCTL::exp_time);

    //TPEVCTL mapping
    py::class_<TPEVCTL>(m, "TPEVCTL")
        .def(py::init([](long flags, const char *name1, const char *name2)
            {
             //auto p = std::make_unique<TPEVCTL>();
             auto p = std::unique_ptr<TPEVCTL>(new TPEVCTL());
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
    m.def("tpenqueue", &ndrxpy_pytpenqueue, 
        R"pbdoc(
        Enqueue message to persistent message queue.

        .. code-block:: python
            :caption: tpenqueue example
            :name: tpenqueue-example

                qctl = e.TPQCTL()
                qctl.corrid=b'\x01\x02'
                qctl.flags=e.TPQCORRID
                qctl1 = e.tpenqueue("SAMPLESPACE", "TESTQ", qctl, {"data":"SOME DATA 1"})

        For more details see **tpenqueue(3)** C API call.

        See **tests/test003_tmq/runtime/bin/tpenqueue.py** for sample code.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments to function (See C descr).
            | **TPETIME** - Queue space call timeout.
            | **TPENOENT** - Queue space not found.
            | **TPESVCFAIL** - Queue space server failed.
            | **TPESVCERR** - Queue space server crashed.
            | **TPESYSTEM** - System error.
            | **TPEOS** - OS error.
            | **TPEBLOCK** - Blocking condition exists and **TPNOBLOCK** was specified.
            | **TPETRAN** - Failed to join global transaction.

        :raise QmException: 
            | Following error codes may be present:
            | **QMEINVAL** - Invalid request buffer. 
            | **QMEOS** - OS error.
            | **QMESYSTEM** - System error.
            | **QMEBADQUEUE** - Bad queue name.

        Parameters
        ----------
        qspace : str
            Queue space name.
        qname : str
            Queue name.
        ctl : TPQCTL
            Control structure.
        data : dict
            Input ATMI data buffer
        flags : int
            Or'd bit flags: **TPNOTRAN**, **TPSIGRSTRT**, **TPNOCHANGE**, 
            **TPTRANSUSPEND**, **TPNOBLOCK**, **TPNOABORT**. Default flag is **0**.

        Returns
        -------
        TPQCTL
            Return control structure (updated with details).

     )pbdoc", py::arg("qspace"), py::arg("qname"), py::arg("ctl"), py::arg("data"),
          py::arg("flags") = 0);

    m.def("tpdequeue", &ndrx_pytpdequeue, 
        R"pbdoc(
        Dequeue message from persistent queue.

        .. code-block:: python
            :caption: tpdequeue example
            :name: tpdequeue-example

                qctl = e.TPQCTL()
                qctl.flags=e.TPQGETBYCORRID
                qctl.corrid=b'\x01\x02'
                qctl, retbuf = e.tpdequeue("SAMPLESPACE", "TESTQ", qctl)
                print(retbuf["data"])

        For more details see **tpdequeue(3)**.

        See **tests/test003_tmq/runtime/bin/tpenqueue.py** for sample code.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments to function (See C descr).
            | **TPENOENT** - Queue space not found (tmqueue process for qspace not started).
            | **TPETIME** - Queue space call timeout.
            | **TPESVCFAIL** - Queue space server failed.
            | **TPESVCERR** - Queue space server crashed.
            | **TPESYSTEM** - System error.
            | **TPEOS** - OS error.
            | **TPEBLOCK** - Blocking condition exists and **TPNOBLOCK** was specified.
            | **TPETRAN** - Failed to join global transaction.

        :raise QmException: 
            | Following error codes may be present:
            | **QMEINVAL** - Invalid request buffer or qctl. 
            | **QMEOS** - OS error.
            | **QMESYSTEM** - System error.
            | **QMEBADQUEUE** - Bad queue name.
            | **QMENOMSG** - No messages in

        Parameters
        ----------
        qspace : str
            Queue space name.
        qname : str
            Queue name.
        ctl : TPQCTL
            Control structure.
        data : dict
            Input ATMI data buffer
        flags : int
            Or'd bit flags: **TPNOTRAN**, **TPSIGRSTRT**, **TPNOCHANGE**, 
            **TPNOTIME**, **TPNOBLOCK**. Default flag is **0**.

        Returns
        -------
        TPQCTL
            Return control structure (updated with details).
        dict
            ATMI data buffer.

     )pbdoc",
          py::arg("qspace"), py::arg("qname"), py::arg("ctl"),
          py::arg("flags") = 0);

    m.def("tpcall", &ndrxpy_pytpcall,
          R"pbdoc(
        Synchronous service call. In case if service returns **TPFAIL** or **TPEXIT**,
        exception is not thrown, instead first return argument shall be tested for
        the tperrno for 0 (to check success case).

        .. code-block:: python
            :caption: tpcall example
            :name: tpcall-example
                import endurox as e

                # Call service with UBF buffer
                tperrno, tpurcode, retbuf = e.tpcall("EXBENCH", { "data":{"T_STRING_FLD":"Hi Jim"}})
                if e.TPESVCFAIL==tperrno:
                    e.tplog_warn("Service failed")
                e.tplog_debug("Service responded %s" % retbuf["data"]["T_STRING_2_FLD"][0])
        
        For more details see **tpcall(3)**.

        :raise AtmiException: 
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
            Input ATMI data buffer
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
            ATMI buffer returned from the server.

     )pbdoc",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpacall", &ndrxpy_pytpacall,           
        R"pbdoc(
        Asynchronous service call. Function returns call descriptor if **TPNOREPLY**
        flag is not set. The replies shall be collected with **tpgetrply()** API
        call by passing the returned call descriptor to the function.
	
        .. code-block:: python
            :caption: tpacall example
            :name: tpacall-example
                import endurox as e

                cd = e.tpacall("EXBENCH", { "data":{"T_STRING_FLD":"Hi Jim"}})
                tperrno, tpurcode, retbuf, cd = e.tpgetrply(cd)
        
        For more details see **tpacall(3)**.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments to function.
            | **TPENOENT** - Service not is advertised.
            | **TPETIME** - Destination queue was full/blocked on time-out expired.
            | **TPESYSTEM** - System error.
            | **TPEOS** - Operating system error.
            | **TPEBLOCK** - Blocking condition found and **TPNOBLOCK** flag was specified
            | **TPEITYPE** - Service error during input buffer handling.

        Parameters
        ----------
        svc : str
            Service name to call
        idata : dict
            Input ATMI data buffer
        flags : int
            Or'd bit flags: **TPNOTRAN**, **TPSIGRSTRT**, **TPNOBLOCK**, 
            **TPNOREPLY**, **TPNOTIME**. Default value is **0**.

        Returns
        -------
        int
            cd - call descriptor. **0** in case if **TPNOREPLY** was specified.

         )pbdoc", py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpgetrply", &ndrxpy_pytpgetrply,
        R"pbdoc(
        Get reply message for asynchronous call initiated by :func:`.tpacall`.
        Exception is throw in case if error occurs other than **TPESVCFAIL**, in
        which case returned `tperrno` value contains **TPESVCFAIL** value.        
	
        .. code-block:: python
            :caption: tpgetrply example
            :name: tpgetrply-example
                import endurox as e

                cd = e.tpacall("EXBENCH", { "data":{"T_STRING_FLD":"Hi Jim"}})
                tperrno, tpurcode, retbuf, cd = e.tpgetrply(cd)
        
        For more details see **tpgetrply(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments to function.
            | **TPEBADDESC** - Call descriptor passed in *cd* is not valid, and 
                **TPGETANY** flag was not specified.
            | **TPETIME** - Destination queue was full/blocked on time-out expired.
            | **TPESVCERR** - Service crashed.
            | **TPEBLOCK** - Blocking condition found and **TPNOBLOCK** flag was specified
            | **TPEITYPE** - Service error during input buffer handling.
            | **TPETRAN** - Service/server failed to start auto-tran.
            | **TPEITYPE** - Buffer type not supported by service.
            | **TPESYSTEM** - System error.
            | **TPEOS** - Operating system error.

        Parameters
        ----------
        cd : int
            Call descriptor. Value is ignored in case if **TPGETANY** flag is
            passed in *flags*.
        flags : int
            Or'd bit flags: **TPGETANY**, **TPNOBLOCK**, **TPSIGRSTRT**, 
            **TPNOTIME**, **TPNOCHANGE**, **TPNOABORT**. Default value is **0**.

        Returns
        -------
        int
            tperrno - error code (**0** or **TPESVCFAIL**)
        int
            tpurcode - code passed to **tpreturn(3)** by the server
        dict
            ATMI buffer returned from the server.
         )pbdoc", 
         py::arg("cd"), py::arg("flags") = 0);

    m.def(
    "tpcancel",
        [](int cd)
        {
            py::gil_scoped_release release;

            if (tpcancel(cd) == EXFAIL)
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Cancel asynchronous call. In case if call descriptor was not issued,
        error is not returned.	
        
        For more details see **tpcancel(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEBADDESC** - *cd* is out of the range of valid values.
            | **TPEINVAL** - Enduro/X is not configured.
            | **TPESYSTEM** - System error.
            | **TPEOS** - Operating system error.

        Parameters
        ----------
        cd : int
            Call descriptor returned by :func:`.tpacall`
         )pbdoc",
    py::arg("cd") = 0);

    //Conversational APIs
    m.def("tpconnect", &ndrxpy_pytpconnect,
        R"pbdoc(
        Connect to conversational service. Connection provides half-duplex
        data streaming between client and service/server where data exchange is
        organized by using :func:`.tpsend` and :func:`.tprecv` ATMI calls.
        
        For more details see **tpconnect(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid arguments passed to function.
            | **TPENOENT** - *svc* service is not available.
            | **TPELIMIT** - Max number of open connections are reached.
            | **TPESVCERR** - Service crashed.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.
            | **TPETRAN** - Destination service was unable to start global transaction.
            | **TPEITYPE** - Destination server does not accept buffer type sent.

        Parameters
        ----------
        svc : str
            Service name to connect to.
        idata : dict
            ATMI buffer to send in connection request.
        flags : int
            Bitwise or'd **TPNOTRAN**, **TPSIGRSTRT**, **TPNOTIME**, **TPSENDONLY**,
            **TPRECVONLY**.
         )pbdoc",
        py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpsend", &ndrxpy_pytpsend,
        R"pbdoc(
        Send conversational data to connected peer. In case if **TPEEVENT** error
        is received, exception is not thrown, instead event code is loaded into
        *revent* return value. In case if event is not generated, *revent* is set
        to **0**.

        .. code-block:: python
            :caption: tpsend example
            :name: tpsend-example
                import endurox as e

                cd = e.tpconnect("CONVSV", {"data":"HELLO"}, e.TPSENDONLY)
                tperrno, revent = e.tpsend(cd, {"data":"HELLO"})
        
        For more details see **tpsend(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid call descriptor.
            | **TPETIME** - Queue was blocked and it timeout out.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.
            | **TPEPROTO** -  Protocol error is generated if given process is 
                in receiver (**TPRECVONLY**) mode.
            | **TPEBLOCK** - **TPNOBLOCK** flag was set and message queue was full.

        Parameters
        ----------
        cd : int
            Conversation descriptor.
        idata : dict
            ATMI buffer to send.
        flags : int
            Bitwise or'd **TPRECVONLY**, **TPNOBLOCK**, **TPSIGRSTRT**, **TPNOTIME**.

        Returns
        -------
        int
            tperrno - error code (**0** or **TPEEVENT**). For other errors, exceptions
                thrown.
        int
            tpurcode - return code passed to :func:`tpreturn`. Value is loaded in case
                if *revent* returned is **TPEV_SVCFAIL** or **TPEV_SVCSUCC**, otherwise
                previous tpurcode is returned.
        int
            revent - In case if **TPEEVENT** tperrno was returned, may contain:
            **TPEV_DISCONIMM**, **TPEV_SENDONLY**, **TPEV_SVCERR**, **TPEV_SVCFAIL**,
            **TPEV_SVCSUCC**.

         )pbdoc",
          py::arg("cd"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tprecv", &ndrxpy_pytprecv, 
        R"pbdoc(
        Receive conversation data block. In case if **TPEEVENT** error
        is received, exception is not thrown, instead event code is loaded into
        *revent* return value. In case if event is not generated, *revent* is set
        to **0**.

        .. code-block:: python
            :caption: tprecv example
            :name: tprecv-example
                import endurox as e

                cd = e.tpconnect("CONVSV", {"data":"HELLO"}, e.TPRECVONLY)
                tperrno, tpurcode, revent = e.tprecv(cd)
        
        For more details see **tprecv(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid call descriptor.
            | **TPETIME** - Queue was blocked and it timeout out.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.
            | **TPEPROTO** -  Protocol error is generated if given process is 
                in receiver (**TPRECVONLY**) mode.
            | **TPEBLOCK** - **TPNOBLOCK** flag was set and message queue was full.

        Parameters
        ----------
        cd : int
            Conversation descriptor.
        idata : dict
            ATMI buffer to send.
        flags : int
            Bitwise or'd **TPNOBLOCK**, **TPSIGRSTRT**, **TPNOTIME**.

        Returns
        -------
        int
            tperrno - error code (**0** or **TPEEVENT**). For other errors, exceptions
                thrown.
        int
            tpurcode - return code passed to :func:`tpreturn`. Value is loaded in case
                if *revent* returned is **TPEV_SVCFAIL** or **TPEV_SVCSUCC**, otherwise
                previous tpurcode is returned.
        int
            revent - In case if **TPEEVENT** tperrno was returned, may contain:
                **TPEV_DISCONIMM**, **TPEV_SENDONLY**, **TPEV_SVCERR**, **TPEV_SVCFAIL**,
                **TPEV_SVCSUCC**.
        dict
            ATMI buffer send by peer.
         )pbdoc",
          py::arg("cd"), py::arg("flags") = 0);

    m.def(
    "tpdiscon",
    [](int cd)
    {
        py::gil_scoped_release release;

        if (tpdiscon(cd) == EXFAIL)
        {
            throw atmi_exception(tperrno);
        }
    },
    R"pbdoc(
        Force disconnect from the conversation session. May be called
        by any peer for the conversation.
        
        For more details see **tpsend(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid conversation descriptor.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        cd : int
            Conversation descriptor.
         )pbdoc",
        py::arg("cd") = 0);
    
    //notification API.
    m.def("tpnotify", &ndrxpy_pytpnotify,
        R"pbdoc(
        Send unsolicited notification to the client process.

        For more details see **tpnotify(3)** C API call.

        .. code-block:: python
            :caption: tpnotify example
            :name: tpnotify-example
                #!/usr/bin/env python3

                import sys
                import endurox as e

                class Server:

                    def tpsvrinit(self, args):
                        e.userlog('Server startup')
                        e.tpadvertise('NOTIFSV', 'NOTIFSV', self.NOTIFSV)
                        return 0

                    def tpsvrdone(self):
                        e.userlog('Server shutdown')
                        
                    def NOTIFSV(self, args):
                        e.tpnotify(args.cltid, {"data":"HELLO WORLD"}, 0)
                        return e.tpreturn(e.TPSUCCESS, 0, {})

                if __name__ == '__main__':
                    e.run(Server(), sys.argv)

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid environment or invalid parameters.
            | **TPENOENT** - Local process queue does not exist.
            | **TPETIME** - Destination queue was blocking and timeout expired.
            | **TPEBLOCK** - Destination queue was blocking and **TPNOBLOCK** was specified.
            | **TPESYSTEM** -  System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        clientid : CLIENTID
            Client ID, as received from service call in *args.cltid*.
        idata : dict
            ATMI buffer to send.
        flags : int
            Bitwise or'd **TPNOBLOCK**, **TPSIGRSTRT**, **TPNOTIME**, **TPACK**.

         )pbdoc",
          py::arg("clientid"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpbroadcast", &ndrxpy_pytpbroadcast, 
        R"pbdoc(
        Broadcast unsolicited message to several processes at once.

        For more details see **tpbroadcast(3)** C API call.

        .. code-block:: python
            :caption: tpbroadcast example
            :name: tpbroadcast-example

            import endurox as e
            e.tpbroadcast("", "", "python3", {})

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid environment or invalid parameters.
            | **TPESYSTEM** -  System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        lmid : str
            Cluster node id. In case of **TPREGEXMATCH** flag several nodes
            may be matched with regexp.
        usrname : str
            RFU.
        cltname : str
            Client process binary name. In case of **TPREGEXMATCH** flag several nodes
            may be matched with regexp.
        idata : dict
            Input ATMI buffer to be delivered to matched processes and nodes.
        flags : int
            Bitwise or'd **TPNOBLOCK**, **TPSIGRSTRT**, **TPNOTIME**, **TPREGEXMATCH**.

         )pbdoc",
          py::arg("lmid"), py::arg("usrname"), py::arg("cltname"), 
          py::arg("idata"), py::arg("flags") = 0);

    m.def("tpsetunsol", [](const py::object &func) { ndrxpy_pytpsetunsol(func); }, 
        R"pbdoc(
        Set unsolicited message callback handler. Handler receives matched messages posted
        by :func:`.tpnotify` and :func:`.tpbroadcast`. Note that in handler only limited ATMI
        processing may be done. See C API descr.
        Note that callback handler is associated with the ATMI context. If using several
        contexts or threads, each of them shall be initialized.

        For more details see **tpsetunsol(3)** C API call.

        .. code-block:: python
            :caption: tpsetunsol example
            :name: tpsetunsol-example

            import unittest
            import endurox as e

            class TestTpnotify(unittest.TestCase):

                cnt = 0

                def unsol_handler(self, data):
                    if data["data"] == "HELLO WORLD":
                        TestTpnotify.cnt=TestTpnotify.cnt+1

                # NOTIFSV publishes one notification to this CLIENTID.
                def test_tpnotify(self):
                    e.tpsetunsol(self.unsol_handler)
                    tperrno, tpurcode, retbuf = e.tpcall("NOTIFSV", { "data":{"T_STRING_FLD":"Hi Jim"}})
                    self.assertEqual(1, TestTpnotify.cnt)
                    e.tpterm()

            if __name__ == '__main__':
                unittest.main()

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid environment or invalid parameters.
            | **TPESYSTEM** -  System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        func : callbackFunction(data) -> None
            Callback function to be invoked when unsolicited message is received
            by the process. *data* parameter is standard ATMI buffer.

            )pbdoc",
          py::arg("func"));
    m.def(
        "tpchkunsol",
        []()
        {
            py::gil_scoped_release release;

            int ret = tpchkunsol();
            if (EXFAIL==ret)
            {
                throw atmi_exception(tperrno);
            }
            return ret;
        }, 
        R"pbdoc(
        Check and process (do callback) unsolicited messages delivered by 
        :func:`.tpnotify` and :func:`.tpbroadcast` to given process.

        For more details see **tpchkunsol(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPESYSTEM** -  System error occurred.
            | **TPEOS** - Operating system error occurred.

        Returns
        -------
        cnt : int
            | Number of unsolicited messages processed.

            )pbdoc");

    m.def("tpexport", &ndrxpy_pytpexport,
                 R"pbdoc(
        Export ATMI buffer. **NULL** buffer export is not supported.

        .. code-block:: python
            :caption: tpexport example
            :name: tpexport-example

            import endurox as e
            buf = e.tpexport({"data":"HELLO WORLD"})
            print(buf)
            # will print:
            # b'{"buftype":"STRING","version":1,"data":"HELLO WORLD"}'
            

        For more details see **tpexport(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid buffer passed.
            | **TPEOTYPE** -  Invalid input type.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        ibuf : dict
            | ATMI buffer.
        flags : int
            | Bitwise flags, may contain **TPEX_STRING**. Default is **0**.

        Returns
        -------
        buf_serial : object
            By default function returns byte array. If *flags* did contain **TPEX_STRING**,
            output format is in Base64 and returned value type is string.

            )pbdoc"
          , py::arg("ibuf"), py::arg("flags") = 0);

    m.def("tpimport", &ndrxpy_pytpimport,
                 R"pbdoc(
        Import previously exported ATMI buffer. If it was exported with :data:`.TPEX_STRING` flag,
        then this function shall be invoked with this flag too.

        .. code-block:: python
            :caption: tpimport example
            :name: tpimport-example

            import endurox as e
            buf_exp = e.tpexport({"data":"HELLO WORLD"}, e.TPEX_STRING)
            buf_org = e.tpimport(buf_exp, e.TPEX_STRING)
            

        For more details see **tpimport(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid parameters.
            | **TPEOTYPE** -  Invalid input type.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        istr : str
            | Serialized buffer with :func:`.tpexport`
        flags : int
            | Bitwise flags, may contain :data:`.TPEX_STRING`, :data:`.TPEX_NOCHANGE`. Default is **0**.

        Returns
        -------
        buf : dict
            Restored ATMI buffer.
            )pbdoc"
          , py::arg("istr"), py::arg("flags") = 0);

    m.def("tppost", &ndrxpy_pytppost,
                 R"pbdoc(
        Post event to the event broker.

        .. code-block:: python
            :caption: tppost example
            :name: tppost-example

            import endurox as e
            # post event with UBF buffer
            cnt = e.tppost("TESTEV", {"data":{"T_STRING_FLD":"HELLO EVENT"}}, 0)
            print("Applied %d" % cnt)
            
        For more details see **tppost(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid parameters.
            | **TPENOENT** -  Event server (**tpevsrv(3)**) is not started.
            | **TPETIME** - Event server timeout out.
            | **TPESVCFAIL** - Event server failure.
            | **TPESVCERR** - Event server has crashed.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        eventname : str
            | Event name.
        data : dict
            | ATMI buffer to post.
        flags : int
            | Bitwise flags, may contain :data:`.TPNOTRAN`, :data:`.TPNOREPLY`, :data:`.TPSIGRSTRT`, :data:`.TPNOTIME` and :data:`.TPNOBLOCK` 
            | Default is **0**.

        Returns
        -------
        cnt : int
            Number of ATMI servers consumed the event.
            )pbdoc"
          , py::arg("eventname"),
          py::arg("data"), py::arg("flags") = 0);

    m.def(
        "tpgblktime",
        [](long flags)
        {
            int rc = tpgblktime(flags);
            if (rc == -1)
            {
                throw atmi_exception(tperrno);
            }
            return rc;
        },
        R"pbdoc(
        Get current ATMI call timeout setting configure for thread.
            
        For more details see **tpgblktime(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags.
            | **TPESYSTEM** - System error occurred.

        Parameters
        ----------
        flags : int
            | Bitwise flags, may contain :data:`.TPBLK_ALL`, :data:`.TPBLK_NEXT`.

        Returns
        -------
        tout : int
            Current timeout configured for thread. If no timeout is configure
            value **0** is returned.
            )pbdoc"
        , py::arg("flags"));

    m.def(
        "tpsblktime",
        [](int blktime, long flags)
        {
            if (tpsblktime(blktime, flags) == -1)
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Set ATMI call timeout value. Setting affects current thread/ATMI context.
        Process level timeout may be applied by :func:`.tptoutset` function.
            
        For more details see **tpsblktime(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags or timeout value.

        Parameters
        ----------
        blktime : int
            | Timeout value in seconds.
        flags : int
            | :data:`.TPBLK_ALL` or :data:`TPBLK_NEXT`.
            )pbdoc"
        , py::arg("blktime"), py::arg("flags"));

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
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Joins thread to application
        
        For more deatils see C call *tpinit(3)*.

        :raise AtmiException: 
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
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Leaves application, closes ATMI session.
        
        For more details see C call *tpterm(3)*.

        :raise AtmiException: 
            | Following error codes may be present:
            | *TPEPROTO* - Called from ATMI server (main thread),
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
                throw atmi_exception(tperrno);
            }
        },
         R"pbdoc(
        Start global transaction.

        For more details see **tpbegin(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags passed.
            | **TPETIME** - Transaction manager (**tmsrv(8)**) timeout out.
            | **TPESVCERR** - Transaction manager crashed.
            | **TPEPROTO** - Invalid operations sequence.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        timeout : int
            | Transaction timeout value in seconds.
        flags : int
            | RFU. Shall be set to **0**, which is default value.

         )pbdoc"
         , py::arg("timeout"), py::arg("flags") = 0);
    m.def(
        "tpsuspend",
        [](long flags)
        {
            TPTRANID tranid;
            py::gil_scoped_release release;
            if (tpsuspend(&tranid, flags) == -1)
            {
                throw atmi_exception(tperrno);
            }
            return pytptranid(reinterpret_cast<char *>(&tranid), sizeof(tranid));
        },
        R"pbdoc(
        Suspend global transaction.

        For more details see **tpsuspend(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags passed.
            | **TPEPROTO** - Invalid operations sequence.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        flags : int
            | Bitwise flags of :data:`.TPTXNOOPTIM` and :data:`.TPTXTMSUSPEND`.
            | default value is **0**.

        Returns
        -------
        tid : TPTRANID
            Suspend transaction identifier

         )pbdoc", py::arg("flags") = 0);

    m.def(
        "tpresume",
        [](pytptranid tranid, long flags)
        {
            py::gil_scoped_release release;
            if (tpresume(reinterpret_cast<TPTRANID *>(
#if PY_MAJOR_VERSION >= 3
                             PyBytes_AsString(tranid.tranid.ptr())
#else
                             PyString_AsString(tranid.tranid.ptr())
#endif
                                 ),
                         flags) == -1)
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Resume previously suspended global transaction.

        For more details see **tpresume(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags passed.
            | **TPEPROTO** - Invalid operations sequence.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        tranid : TPTRANID
            Transaction identifier returned by :func:`.tpsuspend`.
        flags : int
            | Bitwise flags of :data:`.TPTXNOOPTIM` and :data:`.TPTXTMSUSPEND`.
            | default value is **0**.

        Returns
        -------
        tid : TPTRANID
            Suspend transaction identifier

         )pbdoc", py::arg("tranid"), py::arg("flags") = 0);

    m.def(
        "tpcommit",
        [](long flags)
        {
            py::gil_scoped_release release;
            if (tpcommit(flags) == -1)
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Commit global transaction currently associated with given thread.

        For more details see **tpcommit(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags passed.
            | **TPETIME** - Transaction manager timeout.
            | **TPEABORT** - Global transaction was aborted (due to marking or
                error error during two phase commit).
            | **TPEHAZARD** - Partial commit and/or abort.
            | **TPEHEURISTIC** - Partial commit and/or abort.
            | **TPEPROTO** - Invalid call sequence.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        flags : int
            | Bitwise flags of :data:`.TPTXCOMMITDLOG` default value is **0**.

         )pbdoc", py::arg("flags") = 0);

    m.def(
        "tpabort",
        [](long flags)
        {
            py::gil_scoped_release release;
            if (tpabort(flags) == -1)
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Abort global transaction.

        For more details see **tpabort(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid flags passed.
            | **TPETIME** - Transaction manager timeout.
            | **TPEABORT** - Global transaction was aborted (due to marking or
                error error during two phase commit).
            | **TPEHAZARD** - Partial commit and/or abort.
            | **TPEHEURISTIC** - Partial commit and/or abort.
            | **TPEPROTO** - Invalid call sequence.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

        Parameters
        ----------
        flags : int
            | RFU, default value is **0**.

         )pbdoc", py::arg("flags") = 0);

    m.def(
        "tpgetlev",
        []()
        {
            int rc;
            if ((rc = tpgetlev()) == -1)
            {
                throw atmi_exception(tperrno);
            }
            return py::bool_(rc);
        },
        R"pbdoc(
        Get global transaction status.

        For more details see **tpgetlev(3)** C API call.

        Parameters
        ----------
        flags : int
            | RFU, default value is **0**.

        Returns
        -------
        status : bool
            Value **False** is returned if correct thread is not part of the global
            transaction. Value **True** is returned in case if thread is in global
            transaction.
         )pbdoc");

    m.def(
        "tpopen",
        []()
        {
            if (EXFAIL==tpopen())
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Open XA sub-system. Function loads necessary drivers and connects to
        resource manager.

        For more details see **tpopen(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPERMERR** - Resource manager error.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

         )pbdoc");
    m.def(
        "tpclose",
        []()
        {
            if (EXFAIL==tpclose())
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Close XA sub-system previously open by :data:`.tpopen`

        For more details see **tpclose(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEPROTO** - Thread is in global transaction.
            | **TPERMERR** - Resource manager error.
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating system error occurred.

         )pbdoc");
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
                    throw atmi_exception(tperrno);
                }
            }

            return py::bytes(tmp.buf, tmp.size);
        },
        R"pbdoc(
        Encrypt data block (byte array).

        For more details see **tpencrypt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid input data.


        **Parameters:**

        input : bytes
            | Data to encrypt.
        flags : int
            | Shall be set to **0** (default).

        **Returns:**

        value : bytes
            | Encrypted value block

         )pbdoc",
        py::arg("input"), py::arg("flags")=0);

    //Having issuing with sphinx and overloaded functions
    //Such as "CRITICAL: Unexpected section title"
    //thus we have manual titles... (just bold)
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
                    throw atmi_exception(tperrno);
                }
            }

            return py::str(tmp.buf);
        },
        R"pbdoc(
        Encrypt string.

        For more details see **tpencrypt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid input data.

        **Parameters:**

        input : str
            | Data to encrypt, string.
        flags : int
            | Shall be set to **0** (default).

        **Returns:**

        value : str
            | Encrypted value, Base64 string

         )pbdoc",
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
                    throw atmi_exception(tperrno);
                }
            }
            return py::bytes(tmp.buf, tmp.size);
        },
        R"pbdoc(
        Decrypt byte array. Original value was encrypted with :func:`.tpencrypt`, byte array version.

        For more details see **tpdecrypt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid input data.
            | **TPEOS** - System error occurred.

        **Parameters:**

        input : bytes
            | Encrypted data.
        flags : int
            | Shall be set to **0** (default).

        **Returns:**

        value : bytes
            | Decrypted value.

         )pbdoc",
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
                    throw atmi_exception(tperrno);
                }
            }

            return py::str(tmp.buf);
        },
        R"pbdoc(
        Decrypt string. Original value was encrypted with :func:`.tpencrypt`, string version.

        For more details see **tpdecrypt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Invalid input data.
            | **TPEOS** - System error occurred.

        **Parameters:**

        input : str
            | Base64 encrypted data.
        flags : int
            | Shall be set to **0** (default).

        **Returns:**

        value : str
            | Decrypted value.

         )pbdoc",
        py::arg("input"), py::arg("flags")=0);

    m.def(
        "tuxgetenv",
        [](std::string envname)
        {
            return py::str(tuxgetenv(const_cast<char *>(envname.c_str())));
        },
        R"pbdoc(
        Get environment variable. This function directly uses libc getenv() function (i.e. avoids
        Python env variable cache). Use this function to access any [@global] settings applied
        from Enduro/X ini config.

        For more details see **tuxgetenv(3)** C API call.

        Parameters
        ----------
        envname : str
            | Environment name.

        Returns
        -------
        env_val : str
            | Environment variable value.

         )pbdoc",
        py::arg("envname"));

    m.def(
        "tpnewctxt",
        [](bool auto_destroy, bool auto_set)
        {
            auto ctxt = tpnewctxt(auto_destroy, auto_set);
            return pytpcontext(&ctxt);
        },
        R"pbdoc(
        Create new ATMI Context.

        For more details see **tpnewctxt(3)** C API call.

        Parameters
        ----------
        auto_destroy : bool
            | If set to **true**, delete the Context when current thread exits.
        auto_set : bool
            | If set to **true**, associate current thread with created context.

        Returns
        -------
        context : TPCONTEXT_T
            | ATMI Context handle.

         )pbdoc",
        py::arg("auto_destroy"), py::arg("auto_set"));

    m.def(
        "tpgetctxt",
        [](long flags)
        {
            TPCONTEXT_T ctxt;
            if (EXFAIL==tpgetctxt(&ctxt, flags))
            {
                throw atmi_exception(tperrno);
            }

            return pytpcontext(&ctxt);
        },
        R"pbdoc(
        Retrieve current ATMI context handle and put current thread
        in **TPNULLCONTEXT** context.

        For more details see **tpgetctxt(3)** C API call.

        Parameters
        ----------
        flags : int
            | RFU, default **0**.

        Returns
        -------
        context : TPCONTEXT_T
            | ATMI Context handle.

         )pbdoc",
        py::arg("flags")=0);

    m.def(
        "tpsetctxt",
        [](pytpcontext *context, long flags)
        {
            TPCONTEXT_T ctxt;
            context->getCtxt(&ctxt);
            if (EXSUCCEED!=tpsetctxt(ctxt, flags))
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Set current ATMI context from handle received from :func:`.tpgetctxt`
        or :func:`.tpnewctxt`.

        For more details see **tpsetctxt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPENOENT** - Invalid context data.
            | **TPESYSTEM** - System error occurred.

        **Parameters**

        context : TPCONTEXT_T
            | Context handle.
        flags : int
            | RFU, default **0**.

         )pbdoc",
        py::arg("context"), py::arg("flags")=0);

    m.def(
        "tpsetctxt",
        [](py::none none, long flags)
        {
            if (EXSUCCEED!=tpsetctxt(TPNULLCONTEXT, flags))
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Set **TPNULLCONTEXT**. Removes given thread from any ATMI context.

        For more details see **tpsetctxt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPESYSTEM** - System error occurred.

        **Parameters**

        none : none
            | Python's :const:`py::None` constant.
        flags : int
            | RFU, default **0**.

         )pbdoc",
        py::arg("none"), py::arg("flags")=0);

    m.def(
        "tpfreectxt",
        [](pytpcontext *context)
        {
            TPCONTEXT_T ctxt;
            context->getCtxt(&ctxt);

            tpfreectxt(ctxt);
        },
        R"pbdoc(
        Free ATMI context.

        For more details see **tpfreectxt(3)** C API call.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating System error occurred.

        Parameters
        ----------
        context : int
            | ATMI context read by :func:`.tpgetctxt` or :func:`.tpnewctxt`
         )pbdoc",
        py::arg("context"));

    m.def(
        "tpgetnodeid",
        [](void)
        {
            return tpgetnodeid();
        },
        R"pbdoc(
        Return current Enduro/X cluster node id.

        For more details see C call *tpgetnodeid(3)*.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPESYSTEM** - System error occurred.
            | **TPEOS** - Operating System error occurred.

        Returns
        -------
        nodeid : int
            | Enduro/X cluster node id.
         )pbdoc"
        );

    m.def(
        "tpgprio",
        [](void)
        {
            return tpgprio();
        },
        R"pbdoc(
        Get last last service call priority.

        For more details see C call *tpgprio(3)*.

        Returns
        -------
        prio : int
            | Last ATMI service call priority.
     )pbdoc");

    m.def(
        "tpsprio",
        [](int prio, long flags)
        {
            if (EXSUCCEED!=tpsprio(prio, flags))
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Set priority for next ATMI service call. *prio* can be absolute value
        in such case it must be in range of **1..100** (if flag **TPABSOLUTE**
        is used). In relative mode, priority range must be in range of *-100..100*.
        Default mode for *flags* (value **0**) is relative mode.

        Priority is used in **epoll** and **kqueue** poller modes. In other
        modes setting is ignored and no message prioritization is used.

        For more details see C call *tpsprio(3)*.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - *prio* is out of range.

        Parameters
        ----------
        prio : int
            | Service call priority.
        flags : int
            | Flag **TPABSOLUTE**. Default is **0**.
     )pbdoc", py::arg("prio"), py::arg("flags")=0);

    m.def(
        "tpscmt",
        [](long flags)
        {
            int ret;
            if (EXFAIL==(ret=tpscmt(flags)))
            {
                throw atmi_exception(tperrno);
            }
            return ret;
        },
        R"pbdoc(
        Set commit mode, how :func:`tpcommit` returns, either after full two phase
        commit, or only after decision logged (i.e. prepare phase).

        For more details see C call *tpscmt(3)*.

        Returns
        -------
        flags : int
            | **TP_CMT_LOGGED** or **TP_CMT_COMPLETE** (default).
         )pbdoc", py::arg("flags"));

    m.def(
        "tptoutget",
        [](void)
        {
            return tptoutget();
        },
        R"pbdoc(
        Return current ATMI level timeout setting.

        For more details see C call *toutget(3)*.

        Parameters
        -------
        int
            tout - current timeout setting in seconds.
     )pbdoc"
        );

    m.def(
        "tptoutset",
        [](int tout)
        {
            if (EXSUCCEED!=tptoutset(tout))
            {
                throw atmi_exception(tperrno);
            }
        },
        R"pbdoc(
        Set process level ATMI call timeout.
        Setting overrides *NDRX_TOUT* environment setting.

        For more details see C call *tptoutset(3)*.

        :raise AtmiException:
            | Following error codes may be present:
            | **TPEINVAL** - value **0** as passed in *tout*.

        Parameters
        ----------
        tout : int
            | Timeout value in seconds.
     )pbdoc",
        py::arg("tout")
        );

    //Access to XA drivers from cx_Oracle
    m.def(
        "xaoSvcCtx",
        []()
        {
            struct xa_switch_t *sw = ndrx_xa_sw_get();

            if (nullptr!=sw && 0==strcmp(sw->name, "Oracle_XA"))
            {
                /* this is ora */
                ndrx_ora_tpgetconn_t *detail = reinterpret_cast<ndrx_ora_tpgetconn_t *>(tpgetconn());

                if (nullptr!=detail)
                {
                    if (detail->magic!=0x1fca8e4c)
                    {
                        NDRX_LOG(log_error, "Invalid ora lib magic [%x] expected [%x]",
                            detail->magic, 0x1fca8e4c);
                        throw std::runtime_error("Invalid tpgetconn() magic");
                    }

                    if (detail->version<1)
                    {
                        throw std::runtime_error("Expected tpgetconn() version >=1");
                    }

                    if (nullptr==detail->xaoSvcCtx)
                    {
                        throw std::runtime_error("xaoSvcCtx is null");
                    }

                    xao_svc_ctx *xao_svc_ctx_ptr = reinterpret_cast<xao_svc_ctx *>(detail->xaoSvcCtx);

                    return reinterpret_cast<unsigned long long>(
                            (*xao_svc_ctx_ptr)(nullptr));
                }
            }
            throw std::runtime_error("tpinit() not issued, or Oracle drivers not configured");
        },
        R"pbdoc(
        Returns the OCI service handle for a given XA connection.
        Usable only for Oracle DB.

        .. code-block:: python
            :caption: xaoSvcCtx example
            :name: xaoSvcCtx-example

            import endurox as e
            import cx_Oracle

            e.tpinit()            
            e.tpopen()
            db = cx_Oracle.connect(handle=e.xaoSvcCtx())
            e.tpbegin(99, 0)
            
            with db.cursor() as cursor:
                cursor.execute("delete from pyaccounts")
            e.tpcommit(0)
            e.tpterm()


        :raise RuntimeError: 
            | Oracle XA drivers not properly initialized.

        Returns
        -------
        handle : int
            Oracle OCI service handle.
            )pbdoc");
    }

/* vim: set ts=4 sw=4 et smartindent: */
