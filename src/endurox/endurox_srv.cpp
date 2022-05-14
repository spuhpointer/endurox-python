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
        tpreturn(TPEXIT, 0, nullptr, 0, 0);
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
exprivate py::bytes ndrxpy_tpsrvgetctxdata(void)
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

    auto ret = py::bytes(buf, len);

    tpsrvfreectxdata(buf);

    return ret;
}

/**
 * @brief Restore context data in the worker thread
 * 
 * @param flags flags
 */
exprivate void ndrxpy_tpsrvsetctxdata(py::bytes ctxt, long flags)
{
    std::string val(PyBytes_AsString(ctxt.ptr()), PyBytes_Size(ctxt.ptr()));
    
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


/** TODO:  xaoSvcCtx shall be returned from tpgetconn() 
 * as extension from oracle loader lib
 */
struct tmsvrargs_t *_tmgetsvrargs(const char *rmname)
{
    tmsvrargs.reserved1 = nullptr;
    tmsvrargs.reserved2 = nullptr;
    if (strcasecmp(rmname, "NONE") == 0)
    {
        tmsvrargs.xa_switch = &tmnull_switch;
    }
    else if (strcasecmp(rmname, "Oracle_XA") == 0)
    {
        const char *orahome = getenv("ORACLE_HOME");
        auto lib =
            std::string((orahome == nullptr ? "" : orahome)) + "/lib/libclntsh.so";
        void *handle = dlopen(lib.c_str(), RTLD_NOW);
        if (!handle)
        {
            throw std::runtime_error(
                std::string("Failed loading $ORACLE_HOME/lib/libclntsh.so ") +
                dlerror());
        }
        tmsvrargs.xa_switch =
            reinterpret_cast<xa_switch_t *>(dlsym(handle, "xaosw"));
        if (tmsvrargs.xa_switch == nullptr)
        {
            throw std::runtime_error("xa_switch_t named xaosw not found");
        }
        xao_svc_ctx_ptr =
            reinterpret_cast<xao_svc_ctx *>(dlsym(handle, "xaoSvcCtx"));
        if (xao_svc_ctx_ptr == nullptr)
        {
            throw std::runtime_error("xa_switch_t named xaosw not found");
        }
    }
    else
    {
        throw std::invalid_argument("Unsupported Resource Manager");
    }
    return &tmsvrargs;
}

expublic void ndrxpy_pyrun(py::object svr, std::vector<std::string> args,
                  const char *rmname)
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
        (void)_tmstartserver(args.size(), &argv[0], _tmgetsvrargs(rmname));
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
    // Service call info object
    py::class_<pytpsvcinfo>(m, "pytpsvcinfo")
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
        "Routine for advertising a service name", py::arg("svcname"), py::arg("funcname"), py::arg("func"));

    m.def("tpsubscribe", &ndrxpy_pytpsubscribe, "Subscribe to event (by server)",
          py::arg("eventexpr"), py::arg("filter"), py::arg("ctl"), py::arg("flags") = 0);

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
        "Unadvertise service", py::arg("tpunadvertise"));

    m.def("run", &ndrxpy_pyrun, "Run Endurox server", py::arg("server"), py::arg("args"),
          py::arg("rmname") = "NONE");

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

