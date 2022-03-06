
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


xatmibuf::xatmibuf() : pp(&p), len(0), p(nullptr) {}

xatmibuf::xatmibuf(TPSVCINFO *svcinfo)
    : pp(&svcinfo->data), len(svcinfo->len), p(nullptr) {}

xatmibuf::xatmibuf(const char *type, long len) : pp(&p), len(len), p(nullptr)
{
    reinit(type, len);
}

void xatmibuf::reinit(const char *type, long len_)
{
    if (*pp == nullptr)
    {
        len = len_;
        *pp = tpalloc(const_cast<char *>(type), nullptr, len);
        if (*pp == nullptr)
        {
            throw std::bad_alloc();
        }
    }
    else
    {
        /* always UBF? */
        UBFH *fbfr = reinterpret_cast<UBFH *>(*pp);
        Binit(fbfr, Bsizeof(fbfr));
    }
}

/* xatmibuf::xatmibuf(xatmibuf &&other) : xatmibuf() ? */
xatmibuf::xatmibuf(xatmibuf &&other) : xatmibuf()
{
    swap(other);
}

xatmibuf &xatmibuf::operator=(xatmibuf &&other)
{
    swap(other);
    return *this;
}

xatmibuf::~xatmibuf()
{
    if (p != nullptr)
    {
        tpfree(p);
    }
}

char *xatmibuf::release()
{
    char *ret = p;
    p = nullptr;
    return ret;
}

UBFH **xatmibuf::fbfr() { return reinterpret_cast<UBFH **>(pp); }

void xatmibuf::mutate(std::function<int(UBFH *)> f)
{
    while (true)
    {
        int rc = f(*fbfr());
        if (rc == -1)
        {
            if (Berror == BNOSPACE)
            {
                len *= 2;
                *pp = tprealloc(*pp, len);
            }
            else
            {
                throw ubf_exception(Berror);
            }
        }
        else
        {
            break;
        }
    }
}

void xatmibuf::swap(xatmibuf &other) noexcept
{
    std::swap(p, other.p);
    std::swap(len, other.len);
}
