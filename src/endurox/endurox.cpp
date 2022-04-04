
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

struct pytpreply
{
    int rval;
    long rcode;
    py::object data;
    int cd;

    pytpreply(int rval, long rcode, py::object data, int cd = -1)
        : rval(rval), rcode(rcode), data(data), cd(cd) {}
};

static py::object server;

/**
 * @brief Extend the XATMI C struct with python specific fields
 */
struct pytpsvcinfo: TPSVCINFO
{
    pytpsvcinfo(TPSVCINFO *inf)
    {
        NDRX_STRCPY_SAFE(name, inf->name);
        NDRX_STRCPY_SAFE(fname, inf->fname);
        len = inf->len;
        flags = inf->flags;
        cd = inf->cd;
        appkey = inf->appkey;
        CLIENTID cltid;
        memcpy(&cltid, &inf->cltid, sizeof(cltid));
    }
    py::object data;
};

//Mapping of advertised functions
std::map<std::string, py::object> M_dispmap {};

static py::object pytpexport(py::object idata, long flags)
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

static py::object pytpimport(const std::string istr, long flags)
{
    xatmibuf obuf("UBF", istr.size());

    long olen = 0;
    int rc = tpimport(const_cast<char *>(istr.c_str()), istr.size(), obuf.pp,
                      &olen, flags);
    if (rc == -1)
    {
        throw xatmi_exception(tperrno);
    }

    return ndrx_to_py(std::move(obuf));
}

static void pytppost(const std::string eventname, py::object data, long flags)
{
    auto in = ndrx_from_py(data);

    {
        py::gil_scoped_release release;
        int rc =
            tppost(const_cast<char *>(eventname.c_str()), *in.pp, in.len, flags);
        if (rc == -1)
        {
            throw xatmi_exception(tperrno);
        }
    }
}

static pytpreply pytpcall(const char *svc, py::object idata, long flags)
{

    auto in = ndrx_from_py(idata);
    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpcall(const_cast<char *>(svc), *in.pp, in.len, out.pp, &out.len,
                        flags);
        if (rc == -1)
        {
            if (tperrno != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno);
            }
        }
    }
    return pytpreply(tperrno, tpurcode, ndrx_to_py(std::move(out)));
}

static TPQCTL pytpenqueue(const char *qspace, const char *qname, TPQCTL *ctl,
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

static std::pair<TPQCTL, py::object> pytpdequeue(const char *qspace,
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
    return std::make_pair(*ctl, ndrx_to_py(std::move(out)));
}

static int pytpacall(const char *svc, py::object idata, long flags)
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

static pytpreply pytpgetrply(int cd, long flags)
{

    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpgetrply(&cd, out.pp, &out.len, flags);
        if (rc == -1)
        {
            if (tperrno != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno);
            }
        }
    }
    return pytpreply(tperrno, tpurcode, ndrx_to_py(std::move(out)), cd);
}

#define MODULE "endurox"
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

static void pytpreturn(int rval, long rcode, py::object data, long flags)
{
    if (!tsvcresult.clean)
    {
        throw std::runtime_error("tpreturn already called");
    }
    tsvcresult.clean = false;
    tsvcresult.rval = rval;
    tsvcresult.rcode = rcode;
    auto &&odata = ndrx_from_py(data);
    tsvcresult.olen = odata.len;
    tsvcresult.odata = odata.release();
    tsvcresult.forward = false;
}
static void pytpforward(const std::string &svc, py::object data, long flags)
{
    if (!tsvcresult.clean)
    {
        throw std::runtime_error("tpreturn already called");
    }
    tsvcresult.clean = false;
    strncpy(tsvcresult.name, svc.c_str(), sizeof(tsvcresult.name));
    auto &&odata = ndrx_from_py(data);
    tsvcresult.olen = odata.len;
    tsvcresult.odata = odata.release();
    tsvcresult.forward = true;
}

static pytpreply pytpadmcall(py::object idata, long flags)
{
    auto in = ndrx_from_py(idata);
    xatmibuf out("UBF", 1024);
    {
        py::gil_scoped_release release;
        int rc = tpadmcall(*in.fbfr(), out.fbfr(), flags);
        if (rc == -1)
        {
            if (tperrno != TPESVCFAIL)
            {
                throw xatmi_exception(tperrno);
            }
        }
    }
    return pytpreply(tperrno, 0, ndrx_to_py(std::move(out)));
}

int tpsvrinit(int argc, char *argv[])
{
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
    tsvcresult.clean = true;

    try
    {
        py::gil_scoped_acquire acquire;
        auto idata = ndrx_to_py(xatmibuf(svcinfo));

        pytpsvcinfo info(svcinfo);

        info.data = idata;

        auto && func = M_dispmap[svcinfo->fname];

        func(&info);

        if (tsvcresult.clean)
        {
            userlog(const_cast<char *>("tpreturn() not called"));
            tpreturn(TPEXIT, 0, nullptr, 0, 0);
        }
    }
    catch (const std::exception &e)
    {
        userlog(const_cast<char *>("%s"), e.what());
        tpreturn(TPEXIT, 0, nullptr, 0, 0);
    }

    if (tsvcresult.forward)
    {
        tpforward(tsvcresult.name, tsvcresult.odata, tsvcresult.olen, 0);
    }
    else
    {
        tpreturn(tsvcresult.rval, tsvcresult.rcode, tsvcresult.odata,
                 tsvcresult.olen, 0);
    }
}

/**
 * Standard tpadvertise()
 * @param [in] svcname service name
 * @param [in] funcname function name
 * @param [in] func python function pointer
 */
static void pytpadvertise(std::string svcname, std::string funcname, const py::object &func)
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

typedef void *(xao_svc_ctx)(void *);
static xao_svc_ctx *xao_svc_ctx_ptr;
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

static void pyrun(py::object svr, std::vector<std::string> args,
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

    static PyObject *ubfException =
        PyErr_NewException(MODULE ".ubfException", nullptr, nullptr);
    if (ubfException)
    {
        PyTypeObject *as_type = reinterpret_cast<PyTypeObject *>(ubfException);
        as_type->tp_str = EnduroxException_tp_str;
        PyObject *descr = PyDescr_NewGetSet(as_type, EnduroxException_getsetters);
        auto dict = py::reinterpret_borrow<py::dict>(as_type->tp_dict);
        dict[py::handle(((PyDescrObject *)(descr))->d_name)] = py::handle(descr);

        Py_XINCREF(ubfException);
        m.add_object("ubfException", py::handle(ubfException));
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
      PyErr_SetObject(ubfException, args.ptr());
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

    m.def(
        "tpinit",
        [](const char *usrname, const char *cltname, const char *passwd,
           const char *grpname, long flags)
        {
            py::gil_scoped_release release;

            if (tpinit(NULL) == -1)
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Joins an application", py::arg("usrname") = nullptr,
        py::arg("cltname") = nullptr, py::arg("passwd") = nullptr,
        py::arg("grpname") = nullptr, py::arg("flags") = 0);

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

        Parameters
        ----------
        arg1 : int
            Description of arg1
        arg2 : str
            Description of arg2

        Returns
        -------
        int
            Description of return value

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

    m.def("run", &pyrun, "Run Endurox server", py::arg("server"), py::arg("args"),
          py::arg("rmname") = "NONE");

    m.def("tpadmcall", &pytpadmcall, "Administers unbooted application",
          py::arg("idata"), py::arg("flags") = 0);

    m.def("tpreturn", &pytpreturn, "Routine for returning from a service routine",
          py::arg("rval"), py::arg("rcode"), py::arg("data"),
          py::arg("flags") = 0);
    m.def("tpforward", &pytpforward,
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

    m.def("tpenqueue", &pytpenqueue, "Routine to enqueue a message.",
          py::arg("qspace"), py::arg("qname"), py::arg("ctl"), py::arg("data"),
          py::arg("flags") = 0);

    m.def("tpdequeue", &pytpdequeue, "Routine to dequeue a message from a queue.",
          py::arg("qspace"), py::arg("qname"), py::arg("ctl"),
          py::arg("flags") = 0);

    m.def("tpcall", &pytpcall,
          "Routine for sending service request and awaiting its reply",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);

    m.def("tpacall", &pytpacall, "Routine for sending a service request",
          py::arg("svc"), py::arg("idata"), py::arg("flags") = 0);
    m.def("tpgetrply", &pytpgetrply,
          "Routine for getting a reply from a previous request", py::arg("cd"),
          py::arg("flags") = 0);

    m.def("tpexport", &pytpexport,
          "Converts a typed message buffer into an exportable, "
          "machine-independent string representation, that includes digital "
          "signatures and encryption seals",
          py::arg("ibuf"), py::arg("flags") = 0);
    m.def("tpimport", &pytpimport,
          "Converts an exported representation back into a typed message buffer",
          py::arg("istr"), py::arg("flags") = 0);

    m.def("tppost", &pytppost, "Posts an event", py::arg("eventname"),
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
            std::unique_ptr<char, decltype(&free)> guard(
                Bboolco(const_cast<char *>(expression)), &free);
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
            std::unique_ptr<char, decltype(&free)> guard(
                Bboolco(const_cast<char *>(expression)), &free);
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
            std::unique_ptr<char, decltype(&free)> guard(
                Bboolco(const_cast<char *>(expression)), &free);
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
            return ndrx_to_py(std::move(obuf));
        },
        "Builds fielded buffer from printed format", py::arg("iop"));

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

    m.doc() =
        R"pbdoc(
Python3 bindings for writing Endurox clients and servers
--------------------------------------------------------

    .. currentmodule:: endurox

    .. autosummary::
        :toctree: _generate

        tpterm
           

Flags to service routines:

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

Flags to tpreturn:

- TPFAIL - service FAILURE for tpreturn
- TPEXIT - service FAILURE with server exit
- TPSUCCESS - service SUCCESS for tpreturn

Flags to tpsblktime/tpgblktime:

- TPBLK_SECOND - This flag sets the blocktime value, in seconds. This is default behavior.
- TPBLK_NEXT - This flag sets the blocktime value for the next potential blocking API.
- TPBLK_ALL - This flag sets the blocktime value for the all subsequent potential blocking APIs.

Flags to tpenqueue/tpdequeue:

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
