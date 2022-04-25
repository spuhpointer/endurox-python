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
#define NDRXPY_DATA_CALLINFO    "callinfo"  /**< callinfo block             */


#define NDRXPY_DO_DFLT          0           /**< No init                    */
#define NDRXPY_DO_FREE          1           /**< free up buffer recursive   */
#define NDRXPY_DO_NEVERFREE     2           /**< never free up buffer , recu*/

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/

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

    int do_free_ptrs;
    //Recurs free list
    std::map<char *, char *> freelist;
    void recurs_free_fetch();
    void recurs_free();

    /**
     * @brief This is used to release the buffer
     *  in case if processing embedded views, this is set nullptr,
     */
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


typedef void *(xao_svc_ctx)(void *);

/**
 * Reply structore 
 */
struct pytpreply
{
    int rval;
    long rcode;
    py::object data;
    int cd;

    pytpreply(int rval, long rcode, py::object data, int cd = -1)
        : rval(rval), rcode(rcode), data(data), cd(cd) {}
};

/**
 * Reply structure with call descriptor
 */
struct pytpreplycd:pytpreply
{
    using pytpreply::pytpreply;
};

/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/

extern xao_svc_ctx *xao_svc_ctx_ptr;

extern xatmibuf ndrx_from_py(py::object obj);
extern py::object ndrx_to_py(xatmibuf &buf, bool is_master);

//Buffer conversion support:
extern void ndrxpy_from_py_view(py::dict obj, xatmibuf &b, const char *view);
extern py::object ndrxpy_to_py_view(char *cstruct, char *vname, long size);

extern py::object ndrxpy_to_py_ubf(UBFH *fbfr, BFLDLEN buflen);
extern void ndrxpy_from_py_ubf(py::dict obj, xatmibuf &b);

extern void pytpadvertise(std::string svcname, std::string funcname, const py::object &func);
extern void ndrxpy_pyrun(py::object svr, std::vector<std::string> args, const char *rmname);

extern void ndrxpy_pytpreturn(int rval, long rcode, py::object data, long flags);
extern void ndrxpy_pytpforward(const std::string &svc, py::object data, long flags);

extern void ndrxpy_pytpunadvertise(const char * svcname);
extern pytpreply ndrxpy_pytpadmcall(py::object idata, long flags);
extern TPQCTL ndrxpy_pytpenqueue(const char *qspace, const char *qname, TPQCTL *ctl,
                          py::object data, long flags);
extern std::pair<TPQCTL, py::object> ndrx_pytpdequeue(const char *qspace,
                                                 const char *qname, TPQCTL *ctl,
                                                 long flags);
extern pytpreply ndrxpy_pytpcall(const char *svc, py::object idata, long flags);
extern int ndrxpy_pytpacall(const char *svc, py::object idata, long flags);

extern py::object ndrxpy_pytpexport(py::object idata, long flags);
extern py::object ndrxpy_pytpimport(const std::string istr, long flags);

extern pytpreplycd ndrxpy_pytpgetrply(int cd, long flags);
extern int ndrxpy_pytppost(const std::string eventname, py::object data, long flags);
extern long ndrxpy_pytpsubscribe(char *eventexpr, char *filter, TPEVCTL *ctl, long flags);

extern void ndrxpy_register_xatmi(py::module &m);

#endif /* NDRX_PYMOD.H */

/* vim: set ts=4 sw=4 et smartindent: */
