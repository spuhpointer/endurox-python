/**
 * @brief Typed buffer to Python conversion, view type
 *
 * @file bufconv_view.cpp
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

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
namespace py = pybind11;

/**
 * @brief Covert VIEW buffer to python object
 * The output format is similar to UBF encoded in Python dictionary.
 * 
 * @param view_buf C structure of the view
 * @return py::object python dictionary.
 */
expublic py::object ndrxpy_to_py_view(char *view_buf)
{
    throw std::invalid_argument("Not implemented");
}

/**
 * @brief Process single view field
 * 
 * @param buf XATMI buffer where to unload the stuff
 * @param view view name
 * @param cname view field name
 * @param oc  occurrence to set
 * @param obj puthon dict key entry
 */
static void from_py1_view(xatmibuf &buf, const char *view, const char *cname, BFLDOCC oc,
                     py::handle obj)
{
    if (obj.is_none())
    {

#if PY_MAJOR_VERSION >= 3
    }
    else if (py::isinstance<py::bytes>(obj))
    {
        std::string val(PyBytes_AsString(obj.ptr()), PyBytes_Size(obj.ptr()));

        //Set view field finally
        if (EXSUCCEED!=CBvchg(*buf.pp, const_cast<char *>(view), 
                const_cast<char *>(cname), oc, 
                const_cast<char *>(val.data()), val.size(), BFLD_CARRAY))
        {
            throw ubf_exception(Berror);    
        }

#endif
    }
    else if (py::isinstance<py::str>(obj))
    {
#if PY_MAJOR_VERSION >= 3
        py::bytes b = py::reinterpret_steal<py::bytes>(
            PyUnicode_EncodeLocale(obj.ptr(), "surrogateescape"));
        std::string val(PyBytes_AsString(b.ptr()), PyBytes_Size(b.ptr()));
#else
        if (PyUnicode_Check(obj.ptr()))
        {
            obj = PyUnicode_AsEncodedString(obj.ptr(), "utf-8", "surrogateescape");
        }
        std::string val(PyString_AsString(obj.ptr()), PyString_Size(obj.ptr()));
#endif
        if (EXSUCCEED!=CBvchg(*buf.pp, const_cast<char *>(view), 
                    const_cast<char *>(cname), oc, const_cast<char *>(val.data()),
                    val.size(), BFLD_CARRAY))
        {
            throw ubf_exception(Berror); 
        }
    }
    else if (py::isinstance<py::int_>(obj))
    {
        long val = obj.cast<py::int_>();

        if (EXSUCCEED!=CBvchg(*buf.pp, const_cast<char *>(view), 
                    const_cast<char *>(cname), oc, reinterpret_cast<char *>(&val), 0,
                                  BFLD_LONG))
        {
            throw ubf_exception(Berror); 
        }
    }
    else if (py::isinstance<py::float_>(obj))
    {
        double val = obj.cast<py::float_>();
         if (EXSUCCEED!=CBvchg(*buf.pp, const_cast<char *>(view), 
                    const_cast<char *>(cname), oc, reinterpret_cast<char *>(&val), 0,
                                  BFLD_DOUBLE))
        {
            throw ubf_exception(Berror); 
        }
    }
    else
    {
        throw std::invalid_argument("Unsupported type");
    }
}

/**
 * @brief Convert PY to VIEW
 * 
 * @param obj 
 * @param b 
 * @param view VIEW name to process
 */
expublic void ndrxpy_from_py_view(py::dict obj, xatmibuf &b, const char *view)
{
    b.reinit("UBF", nullptr, 1024);
    xatmibuf f;

    for (auto it : obj)
    {
        auto cname = std::string(py::str(it.first));
        py::handle o = it.second;
        if (py::isinstance<py::list>(o))
        {
            BFLDOCC oc = 0;
            for (auto e : o.cast<py::list>())
            {
                from_py1_view(b, view, cname.c_str(), oc++, e);
            }
        }
        else
        {
            // Handle single elements instead of lists for convenience
            from_py1_view(b, view, cname.c_str(), 0, o);
        }
    }
}

/* vim: set ts=4 sw=4 et smartindent: */