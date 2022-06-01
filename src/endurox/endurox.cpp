/**
 * @brief Enduro/X Python module
 *
 * @file endurox.cpp
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

#define MODULE "endurox"


/**
 * tpgetconn() struct values
 * this is copy+paste from unexported header from xadrv/oracle/oracle_common.c (Enduro/X source)
 */
typedef struct
{
    long version;    /**< record version                                */
    void *xaoSvcCtx;    /**< xaoSvcCtx handle,                          */
    
} ndrx_ora_tpgetconn_t;

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

    static PyObject *NstdException =
        PyErr_NewException(MODULE ".NstdException", nullptr, nullptr);
    if (NstdException)
    {
        PyTypeObject *as_type = reinterpret_cast<PyTypeObject *>(NstdException);
        as_type->tp_str = EnduroxException_tp_str;
        PyObject *descr = PyDescr_NewGetSet(as_type, EnduroxException_getsetters);
        auto dict = py::reinterpret_borrow<py::dict>(as_type->tp_dict);
        dict[py::handle(((PyDescrObject *)(descr))->d_name)] = py::handle(descr);

        Py_XINCREF(NstdException);
        m.add_object("NstdException", py::handle(NstdException));
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
    } catch (const nstd_exception &e) {
      py::tuple args(2);
      args[0] = e.what();
      args[1] = e.code();
      PyErr_SetObject(NstdException, args.ptr());
    } });
}

PYBIND11_MODULE(endurox, m)
{
    register_exceptions(m);

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
        "Returns the OCI service handle for a given XA connection");

    ndrxpy_register_ubf(m);
    ndrxpy_register_xatmi(m);
    ndrxpy_register_srv(m);
    ndrxpy_register_tpext(m);
    ndrxpy_register_tplog(m);

    m.attr("TPEV_DISCONIMM") = py::int_(TPEV_DISCONIMM);
    m.attr("TPEV_SVCERR") = py::int_(TPEV_SVCERR);
    m.attr("TPEV_SVCFAIL") = py::int_(TPEV_SVCFAIL);
    m.attr("TPEV_SVCSUCC") = py::int_(TPEV_SVCSUCC);
    m.attr("TPEV_SENDONLY") = py::int_(TPEV_SENDONLY);


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
    m.attr("TPREGEXMATCH") = py::int_(TPREGEXMATCH);

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

    //Enduro/X standard library errors:

    m.attr("NMINVAL") = py::int_(NMINVAL);
    m.attr("NEINVALINI") = py::int_(NEINVALINI);
    m.attr("NEMALLOC") = py::int_(NEMALLOC);
    m.attr("NEUNIX") = py::int_(NEUNIX);
    m.attr("NEINVAL") = py::int_(NEINVAL);
    m.attr("NESYSTEM") = py::int_(NESYSTEM);
    m.attr("NEMANDATORY") = py::int_(NEMANDATORY);
    m.attr("NEFORMAT") = py::int_(NEFORMAT);
    m.attr("NETOUT") = py::int_(NETOUT);
    m.attr("NENOCONN") = py::int_(NENOCONN);
    m.attr("NELIMIT") = py::int_(NELIMIT);
    m.attr("NEPLUGIN") = py::int_(NEPLUGIN);
    m.attr("NENOSPACE") = py::int_(NENOSPACE);
    m.attr("NEINVALKEY") = py::int_(NEINVALKEY);
    m.attr("NENOENT") = py::int_(NENOENT);
    m.attr("NEWRITE") = py::int_(NEWRITE);
    m.attr("NEEXEC") = py::int_(NEEXEC);
    m.attr("NESUPPORT") = py::int_(NESUPPORT);
    m.attr("NEEXISTS") = py::int_(NEEXISTS);
    m.attr("NEVERSION") = py::int_(NEVERSION);
    m.attr("NMAXVAL") = py::int_(NMAXVAL);

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

    m.attr("EXSUCCEED") = py::int_(EXSUCCEED);
    m.attr("EXFAIL") = py::int_(EXFAIL);

    m.attr("TP_CMT_LOGGED") = py::int_(TP_CMT_LOGGED);
    m.attr("TP_CMT_COMPLETE") = py::int_(TP_CMT_COMPLETE);

    //Doc syntax
    //https://www.sphinx-doc.org/en/master/usage/restructuredtext/domains.html#cross-referencing-python-objects
    m.doc() =
        R"pbdoc(
Python3 bindings for writing Endurox clients and servers
########################################################

    .. module:: endurox
    .. currentmodule:: endurox

    .. autosummary::
        :toctree: _generate

        tplogconfig
        tpinit
        tptoutset
        tptoutget
        tpsprio
        tpgprio
        tpgetnodeid
        tpterm
        tpnewctxt
        tpsetctxt
        tpfreectxt
        tuxgetenv
        tpsblktime
        tpgblktime
        tpcall
        tpacall
        tpgetrply
        tpcancel
        tpconnect
        tpsend
        tprecv
        tpdiscon
        tpnotify
        tpbroadcast
        tpsetunsol
        tpchkunsol
        tppost
        tpbegin
        tpsuspend
        tpresume
        tpcommit
        tpabort
        tpgetlev
        tpopen
        tpclose
        tpexport
        tpimport
        tpenqueue
        tpdequeue
        tpscmt
        tpencrypt
        tpdecrypt
        run
        tpsubscribe
        tpunsubscribe
        tpreturn
        tpforward
        tpadvertise
        tpunadvertise
        tpsrvgetctxdata
        tpsrvsetctxdata
        tpcontinue
        tpexit
        Bfldtype
        Bfldno
        Bmkfldid
        Bfname
        Bfldid
        Bboolpr

How to read this documentation
==============================

This documentation contains only short description of the API calls which mentions
core functinoality provided by the API. Each API call contains reference to underlaying
C call which explains in deep details how exactly given function behaves.

XATMI buffer formats
====================

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
-----------------

When building XATMI buffer from Python dictionary, endurox-python library accepts
values to be present as list of values, in such case values are loaded into UBF occurrences
accordingly. value may be presented directly without the list, in such case the value
is loaded into UBF field occurrence **0**.

When XATMI UBF buffer dictionary is received from Enduro/X, all values are loaded into lists,
regardless of did field had several occurrences or just one.

UBF buffer type is selected by following rules:

- *data* key is dictionary and *buftype* key is not present.

- *data* key is dictionary and *buftype* key is set to **UBF**.

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
--------------------

STRING data buffer may contain arbitrary UTF-8 string.
STRING buffer type is selected by following rules:

- *data* key value is string (does not contain 0x00 byte) and *buftype* key is not present.

- *buftype* key is set and contains **STRING** keyword.

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
--------------------

CARRAY buffer type may transport arbitrary byte array.
CARRAY buffer type is selected by following rules:

- *data* key value is byte array and *buftype* key is not present.
- *data* key value is byte array and *buftype* is set to *CARRAY*.

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
------------------

NULL buffers are empty dictionaries, selected by following rules:

- *data* key value is empty dictionary and *buftype* key is not present.
- *data* key value is empty dictionary and *buftype* is set to **NULL**.

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
------------------

JSON buffer type basically is valid UTF-8 string, but with indication that
it contains json formatted data. JSON buffer is selected by following rules:

- *data* is string value and *buftype* is set to **JSON**.

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
------------------

VIEW buffer encodes record/structure data. On the Python side data is encoded in dictionary,
and similarly as with UBF, values may be set as direct values for the dictionary keys
(and are loaded into occurrence 0 of the view field). Or lists may be used to encode
values, if the view field is array, in such case values are loaded in corresponding
occurrences.

When Python code receives VIEW buffer, any NULL fields (as set by **NULL_VAL** see **viewfile(5)**)
are not converted to Python dictionary values, except in case if NULLs proceed valid array values.

For received buffers all values are encapsulated in lists.

VIEW buffer type is selected by following rules:

- *buftype* is set to **VIEW**, *subtype* is set to valid view name and *data* is dictionary.

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

CALL-INFO XATMI buffer association
----------------------------------

Call-info block is additional UBF buffer that may be linked with Any XATMI buffer 
(except **NULL** buffer). The concept behind with call-info block is similar like
HTTP headers information, i.e. additional data linked to the message body.

.. code-block:: python
   :caption: Call info example
   :name: call-info

        import endurox as e

        tperrno, tpurcode, retbuf = e.tpcall("ECHO", { 
                "data":"HELLO STRING"
                , "callinfo":{"T_SHORT_FLD":55, "T_STRING_FLD":"HELLO"}
            })
        print(retbuf)

.. code-block:: python
   :caption: Call info example
   :name: call-info-output

        {'buftype': 'STRING', 'data': 'HELLO STRING'
            , 'callinfo': {'T_SHORT_FLD': [55], 'T_STRING_FLD': ['HELLO']}
        }

Key Classes
===========

This section describes key classes used by Enduro/X API.

TPQCTL
------

This class is used to pass/receive additional information to/from
tpenqueue() and tpdequeue() module function.

.. py:class:: TPQCTL()
   :module: endurox

   Persistent queue API control class

   .. attribute:: flags

      *int* -- See bellow flags

   .. attribute:: deq_time

      *int* -- RFU

   .. attribute:: msgid

      *bytes* -- is assigned by Enduro/X when message is enqueued. 
        Message id is 32 bytes long. When doing dequeue, may specify
        message id to read from Q.

   .. attribute:: diagnostic

      *int* -- See exception codes bellow.

   .. attribute:: diagmsg

      *str* -- Diagnostic messages. Used in **QmException**.

   .. attribute:: priority

      *int* -- RFU.

   .. attribute:: corrid

      *bytes* -- is correlator between messages. ID is 32 bytes long.

   .. attribute:: urcode

      *int* -- RFU.

   .. attribute:: cltid

      *CLIENTID* -- RFU.

   .. attribute:: replyqueue

      *str* -- is queue name where automatic queues may post the
        response provided by destination service.

   .. attribute:: failurequeue

      *str* -- is queue name where failed message 
        (destination automatic service failed all attempts) are enqueued.

   .. attribute:: delivery_qos

      *int* -- RFU.

   .. attribute:: reply_qos

      *int* -- RFU.

   .. attribute:: exp_time

      *int* -- RFU.

:attr:`TPQCTL.flags` may be set to following values:

- **TPQCORRID** - use :attr:`TPQCTL.corrid` identifier, set correlator id when performing
    enqueue.
- **TPQGETBYCORRID** - dequeue message by :attr:`TPQCTL.corrid`.
- **TPQGETBYMSGID** - dequeue message by :attr:`TPQCTL.msgid`.
- **TPQREPLYQ** - use :attr:`TPQCTL.replyqueue` set reply queue for automatic queues.
- **TPQFAILUREQ** - use :attr:`TPQCTL.failurequeue` use failure queue for failed 
    automatic queue messages.

Following :attr:`TPQCTL.diagnostic` (*QmException.code*) codes may be returned:

- **QMEINVAL** - Invalid data.
- **QMEBADRMID** - RFU.
- **QMENOTOPEN** - RFU.
- **QMETRAN** - RFU.
- **QMEBADMSGID** - RFU.
- **QMESYSTEM** - System error.
- **QMEOS** - OS error.
- **QMEABORTED** - RFU.
- **QMENOTA** - RFU.         
- **QMEPROTO** - RFU.
- **QMEBADQUEUE** - Bad queue name.
- **QMENOMSG** - No messages in queue.
- **QMEINUSE** - RFU.
- **QMENOSPACE** - RFU.
- **QMERELEASE** - RFU.
- **QMEINVHANDLE** - RFU.
- **QMESHARE** - RFU.


TPEVCTL
-------

Class used to control event subscription for the XATMI servers.
Used by :func:`.tpsubscribe` and :func:`.tpunsubscribe`.

.. py:class:: TPQCTL()
   :module: endurox

   Event control class

   .. attribute:: flags

      *int* -- Bitwise flags of: **TPEVSERVICE** and **TPEVPERSIST**.

   .. attribute:: name1

      *str* -- Data field 1

   .. attribute:: name2

      *str* -- Data field 2

Flags
=====

Flags to service routines
-------------------------

- **TPNOBLOCK** - non-blocking send/rcv
- **TPSIGRSTRT** - restart rcv on interrupt
- **TPNOREPLY** - no reply expected
- **TPNOTRAN** - not sent in transaction mode
- **TPTRAN** - sent in transaction mode
- **TPNOTIME** - no timeout
- **TPABSOLUTE** - absolute value on tmsetprio
- **TPGETANY** - get any valid reply
- **TPNOCHANGE** - force incoming buffer to match
- **RESERVED_BIT1** - reserved for future use
- **TPCONV** - conversational service
- **TPSENDONLY** - send-only mode
- **TPRECVONLY** - recv-only mode

Flags to tpreturn
-----------------

- **TPFAIL** - service FAILURE for tpreturn
- **TPEXIT** - service FAILURE with server exit
- **TPSUCCESS** - service SUCCESS for tpreturn

Flags to tpsblktime/tpgblktime
------------------------------

- **TPBLK_SECOND** - This flag sets the blocktime value, in seconds. This is default behavior.
- **TPBLK_NEXT** - This flag sets the blocktime value for the next potential blocking API.
- **TPBLK_ALL** - This flag sets the blocktime value for the all subsequent potential blocking APIs.

Flags to tpenqueue/tpdequeue
----------------------------

- **TPQCORRID** - set/get correlation id
- **TPQFAILUREQ** - set/get failure queue
- **TPQBEFOREMSGID** - enqueue before message id
- **TPQGETBYMSGIDOLD** - deprecated
- **TPQMSGID** - get msgid of enq/deq message
- **TPQPRIORITY** - set/get message priority
- **TPQTOP** - enqueue at queue top
- **TPQWAIT** - wait for dequeuing
- **TPQREPLYQ** - set/get reply queue
- **TPQTIME_ABS** - set absolute time
- **TPQTIME_REL** - set absolute time
- **TPQGETBYCORRIDOLD** - deprecated
- **TPQPEEK** - peek
- **TPQDELIVERYQOS** - delivery quality of service
- **TPQREPLYQOS**   - reply message quality of service
- **TPQEXPTIME_ABS** - absolute expiration time
- **TPQEXPTIME_REL** - relative expiration time
- **TPQEXPTIME_NONE**  - never expire
- **TPQGETBYMSGID** - dequeue by msgid
- **TPQGETBYCORRID** - dequeue by corrid
- **TPQQOSDEFAULTPERSIST** - queue's default persistence policy
- **TPQQOSPERSISTENT**  - disk message
- **TPQQOSNONPERSISTENT** - memory message

Flags to tpsubscribe/tpunsubscribe (:attr:`TPEVCTL.flags`)
----------------------------------------------------------

See this const :data:`.TPEVSERVICE`

.. data:: TPEVSERVICE

    Must be present when XATMI server subscribes to event.

.. data:: TPEVPERSIST

    Do not unsubscribe from event in case if service failed
    when event was delivered.

)pbdoc";
}

/* vim: set ts=4 sw=4 et smartindent: */

