/**
 * @brief Enduro/X Python module
 *
 * @file ndrx_pymod.h
 */
/* -----------------------------------------------------------------------------
 * Enduro/X Middleware Platform for Distributed Transaction Processing
 * Copyright (C) 2009-2016, ATR Baltic, Ltd. All Rights Reserved.
 * Copyright (C) 2017-2022, Mavimax, Ltd. All Rights Reserved.
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
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

namespace py = pybind11;


xatmibuf::xatmibuf() : pp(&p), len(0), p(nullptr), do_free_ptrs(NDRXPY_DO_DFLT) {}

xatmibuf::xatmibuf(TPSVCINFO *svcinfo)
    : pp(&p), len(svcinfo->len), p(svcinfo->data), do_free_ptrs(NDRXPY_DO_DFLT) {}

/**
 * @brief Sub-type based allocation
 * 
 * @param type 
 * @param subtype 
 */
xatmibuf::xatmibuf(const char *type, const char *subtype) : pp(&p), len(len), p(nullptr), do_free_ptrs(NDRXPY_DO_DFLT)
{
    reinit(type, subtype, 1024);
}

xatmibuf::xatmibuf(const char *type, long len) : pp(&p), len(len), p(nullptr), do_free_ptrs(NDRXPY_DO_DFLT)
{
    reinit(type, nullptr, len);
}


/**
 * @brief Allocate / reallocate
 * 
 * @param type XATMI type
 * @param subtype XATMI sub-type
 * @param len_ len (where required)
 */
void xatmibuf::reinit(const char *type, const char *subtype, long len_)
{    
    //Free up ptr if have any
    if (nullptr==*pp)
    {
        len = len_;
        *pp = tpalloc(const_cast<char *>(type), const_cast<char *>(subtype), len);
        //For null buffers we can accept NULL return
        if (*pp == nullptr && 0!=strcmp(type, "NULL"))
        {
            NDRX_LOG(log_error, "Failed to realloc: %s", tpstrerror(tperrno));
            throw xatmi_exception(tperrno);
        }

        //Never free?
        if (0==strcmp(type, "UBF") &&
            NDRXPY_DO_DFLT==do_free_ptrs)
        {
            do_free_ptrs=NDRXPY_DO_FREE;
        }
    }
    else
    {
        /* always UBF? 
         * used for recursive buffer processing
         */
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

extern "C" {
    void ndrx_mbuf_Bnext_ptr_first(UBFH *p_ub, Bnext_state_t *state);
}

/**
 * @brief Standard destructor
 * 
 */
xatmibuf::~xatmibuf()
{
    if (p != nullptr)
    {
        tpfree(p);
    }
}

/**
 * @release the buffer (do not destruct, as XATMI already freed)
 * 
 * @return char* 
 */
char *xatmibuf::release()
{
    char *ret = p;

    //Disable destructor
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
    std::swap(do_free_ptrs, other.do_free_ptrs);

    //In case if using differt pp
    if (&other.p!=other.pp)
    {
        std::swap(pp, other.pp);
    }
}

/* vim: set ts=4 sw=4 et smartindent: */

