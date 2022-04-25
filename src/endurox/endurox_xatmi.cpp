
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

/* vim: set ts=4 sw=4 et smartindent: */

