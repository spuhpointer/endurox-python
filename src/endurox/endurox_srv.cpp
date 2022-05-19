/**
 * @brief Enduro/X Python module - xatmi server routines
 *
 * @file endurox_srv.cpp
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
#undef _

#include "exceptions.h"
#include "ndrx_pymod.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>
#include <map>

namespace py = pybind11;


static py::object server;

//Mapping of advertised functions
std::map<std::string, py::object> M_dispmap {};

struct svcresult
{
    int rval;
    long rcode;
    char *odata;
    long olen;
    char name[XATMI_SERVICE_NAME_LENGTH];
    bool forward;
    bool clean;
};
static thread_local svcresult tsvcresult;

expublic void ndrxpy_pytpreturn(int rval, long rcode, py::object data, long flags)
{
    /*
    if (!tsvcresult.clean)
    {
        throw std::runtime_error("tpreturn already called");
    }
    tsvcresult.clean = false;
    */
    tsvcresult.rval = rval;
    tsvcresult.rcode = rcode;
    auto &&odata = ndrx_from_py(data);
    tpreturn(tsvcresult.rval, tsvcresult.rcode, odata.p, odata.len, 0);
    //Normal destructors apply... as running in nojump mode

}
expublic void ndrxpy_pytpforward(const std::string &svc, py::object data, long flags)
{
    /*
    if (!tsvcresult.clean)
    {
        throw std::runtime_error("tpreturn already called");
    }
    tsvcresult.clean = false;
    */
    strncpy(tsvcresult.name, svc.c_str(), sizeof(tsvcresult.name));
    auto &&odata = ndrx_from_py(data);
    tpforward(tsvcresult.name, odata.p, odata.len, 0);

    //Normal destructors apply... as running in nojump mode.
}

extern "C" long G_libatmisrv_flags;

int tpsvrinit(int argc, char *argv[])
{
    py::gil_scoped_acquire acquire;

    /* set no jump, so that we can process recrusive buffer freeups.. */
    G_libatmisrv_flags|=ATMI_SRVLIB_NOLONGJUMP;

    if (hasattr(server, __func__))
    {
        std::vector<std::string> args;
        for (int i = 0; i < argc; i++)
        {
            args.push_back(argv[i]);
        }
        return server.attr(__func__)(args).cast<int>();
    }
    return 0;
}

void tpsvrdone()
{
    py::gil_scoped_acquire acquire;
    if (hasattr(server, __func__))
    {
        server.attr(__func__)();
    }
    M_dispmap.clear();
}

int tpsvrthrinit(int argc, char *argv[])
{

    // Create a new Python thread
    // otherwise pybind11 creates and deletes one
    // and messes up threading.local
    auto const &internals = pybind11::detail::get_internals();
    PyThreadState_New(internals.istate);

    py::gil_scoped_acquire acquire;
    if (hasattr(server, __func__))
    {
        std::vector<std::string> args;
        for (int i = 0; i < argc; i++)
        {
            args.push_back(argv[i]);
        }
        return server.attr(__func__)(args).cast<int>();
    }
    return 0;
}
void tpsvrthrdone()
{
    py::gil_scoped_acquire acquire;
    if (hasattr(server, __func__))
    {
        server.attr(__func__)();
    }
}
/**
 * @brief Server dispatch function
 * 
 * @param svcinfo standard XATMI call descriptor
 */
void PY(TPSVCINFO *svcinfo)
{
    //tsvcresult.clean = true;

    try
    {
        py::gil_scoped_acquire acquire;
        auto ibuf=xatmibuf(svcinfo);
        auto idata = ndrx_to_py(ibuf);

        pytpsvcinfo info(svcinfo);

        info.data = idata;

        auto && func = M_dispmap[svcinfo->fname];

        func(&info);

/*
        if (tsvcresult.clean)
        {
            userlog(const_cast<char *>("tpreturn() not called"));
            tpreturn(TPEXIT, 0, nullptr, 0, 0);
        }
*/
    }
    catch (const std::exception &e)
    {
        NDRX_LOG(log_error, "Got exception at tpreturn: %s", e.what());
        userlog(const_cast<char *>("%s"), e.what());
        /* return service error, soft-err*/
        tpreturn(TPFAIL, TPESVCERR, nullptr, 0, TPSOFTERR);
    }
}

/**
 * Standard tpadvertise()
 * @param [in] svcname service name
 * @param [in] funcname function name
 * @param [in] func python function pointer
 */
expublic void pytpadvertise(std::string svcname, std::string funcname, const py::object &func)
{
    if (tpadvertise_full(const_cast<char *>(svcname.c_str()), PY, 
        const_cast<char *>(funcname.c_str())) == -1)
    {
        throw xatmi_exception(tperrno);
    }

    //Add name mapping to hashmap
    //TODO: might want to check for duplicate advertises, so that function pointers are the same?
    if (M_dispmap.end() == M_dispmap.find(funcname))
    {
        M_dispmap[funcname] = func;
    }

}

/**
 * Unadvertise service
 * @param [in] svcname service name to unadvertise
 */
expublic void ndrxpy_pytpunadvertise(const char *svcname)
{
    if (EXFAIL==tpunadvertise(const_cast<char *>(svcname)))
    {
        throw xatmi_exception(tperrno);
    }

    auto it = M_dispmap.find(svcname);
    if (it != M_dispmap.end()) {
        M_dispmap.erase(it);
    }

}

/**
 * @brief Get server contexts data
 * 
 * @return byte array block 
 */
exprivate struct pytpsrvctxdata ndrxpy_tpsrvgetctxdata(void)
{
    long len=0;
    char *buf;

    {
        py::gil_scoped_release release;

        if (NULL==(buf=tpsrvgetctxdata2(NULL, &len)))
        {
            throw xatmi_exception(tperrno);
        }
    }

    auto ret = pytpsrvctxdata(buf, len);

    tpsrvfreectxdata(buf);

    return ret;
}

/**
 * @brief Restore context data in the worker thread
 * 
 * @param flags flags
 */
exprivate void ndrxpy_tpsrvsetctxdata(struct pytpsrvctxdata* ctxt, long flags)
{
    std::string val(PyBytes_AsString(ctxt->pyctxt.ptr()), PyBytes_Size(ctxt->pyctxt.ptr()));
    
    py::gil_scoped_release release;

    if (EXSUCCEED!=tpsrvsetctxdata (const_cast<char *>(val.data()), flags))
    {
        throw xatmi_exception(tperrno);
    }

}

/**
 * @brief Subscribe to event
 * 
 * @param eventexpr event name
 * @param filter regexp filter
 * @param ctl control struct
 * @param flags any flags
 * @return subscribtion id
 */
expublic long ndrxpy_pytpsubscribe(char *eventexpr, char *filter, TPEVCTL *ctl, long flags)
{
    py::gil_scoped_release release;
    
    long rc = tpsubscribe (eventexpr, filter, ctl, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }
    return rc;
}

//TODO: How about unadvertise?
//However unadvertise is not supported for MT server, thus

extern "C"
{
    extern struct xa_switch_t tmnull_switch;
    extern int _tmbuilt_with_thread_option;
}

static struct tmdsptchtbl_t _tmdsptchtbl[] = {
    {(char *)"", (char *)"PY", PY, 0, 0}, {nullptr, nullptr, nullptr, 0, 0}};

static struct tmsvrargs_t tmsvrargs = {
    nullptr, &_tmdsptchtbl[0], 0, tpsvrinit, tpsvrdone,
    nullptr, nullptr, nullptr, nullptr, nullptr,
    tpsvrthrinit, tpsvrthrdone};

expublic xao_svc_ctx *xao_svc_ctx_ptr;

/**
 * @brief Enduro/X XATMI server main loop entry
 * 
 * @param svr Server object
 * @param args cli args
 */
expublic void ndrxpy_pyrun(py::object svr, std::vector<std::string> args)
{
    server = svr;
    try
    {
        py::gil_scoped_release release;
        _tmbuilt_with_thread_option = 1;
        std::vector<char *> argv(args.size());
        for (size_t i = 0; i < args.size(); i++)
        {
            argv[i] = const_cast<char *>(args[i].c_str());
        }

        _tmbuilt_with_thread_option=EXTRUE;
        struct tmsvrargs_t tmsvrargs =
        {
            NULL,
            &_tmdsptchtbl[0],
            0,
            tpsvrinit,
            tpsvrdone,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            tpsvrthrinit,
            tpsvrthrdone
        };
        
        _tmstartserver( args.size(), &argv[0], &tmsvrargs );

        server = py::none();
    }
    catch (...)
    {
        server = py::none();
        throw;
    }
}
/**
 * @brief Register XATMI server specific functions
 * 
 * @param m Pybind11 module handle
 */
expublic void ndrxpy_register_srv(py::module &m)
{

    //Atmi Context data type
    py::class_<pytpsrvctxdata>(m, "TpSrvCtxtData")
        .def_readonly("pyctxt", &pytpsrvctxdata::pyctxt);

    // Service call info object
    py::class_<pytpsvcinfo>(m, "TpSvcInfo")
        .def_readonly("name", &pytpsvcinfo::name)
        .def_readonly("fname", &pytpsvcinfo::fname)
        .def_readonly("flags", &pytpsvcinfo::flags)
        .def_readonly("appkey", &pytpsvcinfo::appkey)
        .def_readonly("cd", &pytpsvcinfo::cd)
        .def_readonly("cltid", &pytpsvcinfo::cltid)
        /*
        .def("cltid", [](pytpsvcinfo &inf) { 
            return py::bytes(reinterpret_cast<char *>(&inf.cltid), sizeof(inf.cltid)); })
        */
        .def_readonly("data", &pytpsvcinfo::data);

    m.def(
        "tpadvertise", [](const char *svcname, const char *funcname, const py::object &func)
        { pytpadvertise(svcname, funcname, func); },
        R"pbdoc(
        Routine for advertising a service name.

        This function applies to XATMI servers only.

        For more details see C call **tpadvertise(3)**.

        :raise XatmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Service name empty or too long (longer than **MAXTIDENT**)
            | **TPELIMIT** - More than 48 services attempted to advertise by the script.
            | **TPEMATCH** - Service already advertised.
            | **TPEOS** - System error.

        Parameters
        ----------
        svcname : str
            Service name to advertise
        funcname : str
            Function name of the service
        func : object
            Callback function used by service. Callback function receives **data** argument
            which corresponds to **TpSvcInfo** class.
        )pbdoc"
        , py::arg("svcname"), py::arg("funcname"), py::arg("func"));

    m.def("tpsubscribe", &ndrxpy_pytpsubscribe,
        R"pbdoc(
        Subscribe to event. Once event is published by the **tppost(3)**, it is
        delivered to subscribers.

        Service name is specified in **TPEVCTL** class which is following:

        .. code-block:: python
            :caption: TPEVCTL Calss
            :name: TPEVCTL-class

                class TPEVCTL:
                    flags: int
                    name1: str
                    name2: str

        where bitwise *flags* is set to: **TPEVSERVICE** - call service (this must be always set
        for XATMI server). **TPEVPERSIST** is set to not to remove service from event broker
        in case if service failed.

        Service name to which to deliver event notification shall be set in *name1* field.

        For more details see **tpsubscribe(3)** C API call.

        :raise XatmiException:
            | Following error codes may be present:
            | **TPEINVAL** - Service name empty or too long (longer than **MAXTIDENT**)
            | **TPELIMIT** - More than 48 services attempted to advertise by the script.
            | **TPEMATCH** - Service already advertised.
            | **TPEOS** - System error.

        Parameters
        ----------
        eventexpr : str
            Event expression.
        filter : str
            Boolean expression for **UBF** and regexp for **STRING** buffers to test data
            before event delivery.
        ctl : TPEVCTL
            Control structure.
        flags : int
            Bitwise or'd **TPNOTRAN**, **TPSIGRSTRT**, **TPNOTIME** flags.
            
        )pbdoc",
        py::arg("eventexpr"), py::arg("filter"), py::arg("ctl"), py::arg("flags") = 0);
    //TODO: tpunsubscribe.

    //Server contexting:
    m.def("tpsrvgetctxdata", &ndrxpy_tpsrvgetctxdata, "Get service call context data");
    //TODO: TPNOAUTBUF flag is not relevant here, as buffers in py are basically dictionaries
    //and we do not have direct access to underlaying buffer, thus let it restore in the 
    //thread context always.
    m.def("tpsrvsetctxdata", &ndrxpy_tpsrvsetctxdata, "Restore context in the workder thread",
          py::arg("ctxt"), py::arg("flags") = 0);
    m.def("tpcontinue", [](void)
        { tpcontinue(); }, "Let server main thread to proceed without reply");

    m.def(
        "tpunadvertise", [](const char *svcname)
        { ndrxpy_pytpunadvertise(svcname); },
        R"pbdoc(
        Unadvertise service.

        This function applies to XATMI servers only.

        For more details see C call **tpunadvertise(3)**.

        :raise XatmiException:
            | Following error codes may be present:
            | **TPENOENT** - Service not advertised.
            | **TPEOS** - System error.
            | **TPESYSTEM** - Failed to report to **ndrxd(8)**.

        Parameters
        ----------
        svcname : str
            Service name to unadvertise
        )pbdoc",
        py::arg("tpunadvertise"));

    m.def("run", &ndrxpy_pyrun, "Run Endurox server", py::arg("server"), py::arg("args"));

    m.def("tpreturn", &ndrxpy_pytpreturn, 
        R"pbdoc(
        Return from XATMI service call. Any XATMI processing after this call
        shall not be performed, i.e. shall last operation in the XATMI service
        processing.

        This function applies to XATMI servers only.
        
        For more deatils see *tpreturn(3)* C API call.

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
          R"pbdoc(
        Forward control to other service. This shall be last XATMI call
        for the service routine.

        This function applies to XATMI servers only.
        
        For more details see C call *tpforward(3)*.

        Parameters
        ----------
        svc : str
            Name of the target service.
        data : dict
            XATMI buffer returned from the service
        flags : int
            RFU, shall be set to **0**.
        )pbdoc",
          py::arg("svc"), py::arg("data"), py::arg("flags") = 0);    

    m.def(
        "tpexit", [](void)
        { tpexit(); },
        "Restart after return or terminate immediatally (if running from other thread than main)");
}


/* vim: set ts=4 sw=4 et smartindent: */

