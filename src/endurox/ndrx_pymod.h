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

// ndrx_pymod.h
#ifndef NDRX_PYMOD_H
#define NDRX_PYMOD_H

/*---------------------------Includes-----------------------------------*/
#include <atmi.h>
#include <tpadm.h>
#include <userlog.h>
#include <xa.h>
#include <ubf.h>
#include <ndebug.h>
#undef _

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
#define NDRXPY_DATA_DATA        "data"      /**< Actual data field          */
#define NDRXPY_DATA_BUFTYPE     "buftype"   /**< optional buffer type field */
#define NDRXPY_DATA_SUBTYPE     "subtype"   /**< subtype buffer             */
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/

/**
 * XATMI buffer handling routines
 */
class xatmibuf
{
public:
    xatmibuf();
    xatmibuf(TPSVCINFO *svcinfo);
    xatmibuf(const char *type, long len);
    xatmibuf(const char *type, const char *subtype);
    void reinit(const char *type, const char *subtype, long len_);

    xatmibuf(const xatmibuf &) = delete;
    xatmibuf &operator=(const xatmibuf &) = delete;
    
    xatmibuf(xatmibuf &&other);
    xatmibuf &operator=(xatmibuf &&other);
    ~xatmibuf();

    char *release();
    UBFH **fbfr();
    char **pp;
    char *p;
    long len;
    void mutate(std::function<int(UBFH *)> f);

private:
    void swap(xatmibuf &other) noexcept;
};

/**
 * Temporary buffer allocator
 */
class tempbuf
{

public:

    char *buf;
    tempbuf(long size)
    {
        buf = reinterpret_cast<char *>(NDRX_FPMALLOC(size, 0));

        if (NULL==buf)
        {
            throw std::bad_alloc();
        }

    }
    ~tempbuf()
    {
        NDRX_FPFREE(buf);
    }
};

/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

extern xatmibuf ndrx_from_py(py::object obj);
extern py::object ndrx_to_py(xatmibuf buf);

//Buffer conversion support:
extern void ndrxpy_from_py_view(py::dict obj, xatmibuf &b, const char *view);
extern py::object ndrxpy_to_py_view(char *cstruct, char *vname, long size);

extern py::object ndrxpy_to_py_ubf(UBFH *fbfr, BFLDLEN buflen);
extern void ndrxpy_from_py_ubf(py::dict obj, xatmibuf &b);

#endif /* NDRX_PYMOD.H */

/* vim: set ts=4 sw=4 et smartindent: */
