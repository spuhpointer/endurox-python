/**
 * @brief Typed buffer to Python conversion
 *
 * @file bufconv.cpp
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
#include <ndebug.h>
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
 * @brief This will add all XATMI related stuff under the {"data":<XATMI data...>}
 * 
 * @param buf 
 * @return expublic 
 */
expublic py::object ndrx_to_py(xatmibuf buf)
{
    char type[8];
    char subtype[16];

    py::dict result;

    NDRX_LOG(log_debug, "Into ndrx_to_py()");

    if (tptypes(*buf.pp, type, subtype) == -1)
    {
        throw std::invalid_argument("Invalid buffer type");
    }

    //Return buffer sub-type
    result["buftype"] = type;

    if (EXEOS!=subtype[0])
    {
        result["subtype"]=subtype;
    }

    if (strcmp(type, "STRING") == 0)
    {
        result["data"]=py::cast(*buf.pp);
    }
    else if (strcmp(type, "CARRAY") == 0 || strcmp(type, "X_OCTET") == 0)
    {
        result["data"]=py::bytes(*buf.pp, buf.len);
    }
    else if (strcmp(type, "UBF") == 0)
    {
        result["data"]=ndrxpy_to_py_ubf(*buf.fbfr(), 0);
    }
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }

    return result;
}

/**
 * @brief Must be dict with "data" key. So valid buffer is:
 * 
 * {"data":<XATMI_BUFFER>, "buftype":"UBF|VIEW|STRING|JSON|CARRAY|NULL", "subtype":"<VIEW_TYPE>", ["callinfo":{<UBF_DATA>}]}
 * 
 * For NULL buffers, data field is not present.
 * 
 * @param obj Pyton object
 * @return converted XATMI buffer
 */
expublic xatmibuf ndrx_from_py(py::object obj)
{
    std::string buftype = "";
    std::string subtype = "";

    NDRX_LOG(log_debug, "Into ndrx_from_py()");

    if (!py::isinstance<py::dict>(obj))
    {
        throw std::invalid_argument("Unsupported buffer type");
    }
    //Get the data field

    auto dict = static_cast<py::dict>(obj);

    //data i
    auto data = dict[NDRXPY_DATA_DATA];

    if (dict.contains(NDRXPY_DATA_BUFTYPE))
    {
        buftype = py::str(dict[NDRXPY_DATA_BUFTYPE]);
    }
    
    if (dict.contains(NDRXPY_DATA_SUBTYPE))
    {
        subtype = py::str(dict[NDRXPY_DATA_SUBTYPE]);
    }

    /* process JSON data... as string */
    if (buftype=="JSON")
    {
        if (py::isinstance<py::str>(data))
        {
            throw std::invalid_argument("String expected for JSON buftype, got: "+buftype);
        } 

        std::string s = py::str(data);

        xatmibuf buf("JSON", s.size() + 1);
        strcpy(*buf.pp, s.c_str());
        return buf;

    }
    else if (buftype=="VIEW")
    {
        if (subtype=="")
        {
            throw std::invalid_argument("subtype expected for VIEW buffer");
        }

        xatmibuf buf("VIEW", subtype.c_str());

        ndrxpy_from_py_view(static_cast<py::dict>(data), buf, subtype.c_str());

        return buf;
    }
    else if (py::isinstance<py::bytes>(data))
    {
        if (buftype!="" && buftype!="CARRAY")
        {
            throw std::invalid_argument("For byte array data "
                "expected CARRAY buftype, got: "+buftype);
        }
        
        xatmibuf buf("CARRAY", PyBytes_Size(data.ptr()));
        memcpy(*buf.pp, PyBytes_AsString(data.ptr()), PyBytes_Size(data.ptr()));
        return buf;
    }
    else if (py::isinstance<py::str>(data))
    {
        if (buftype!="" && buftype!="STRING")
        {
            throw std::invalid_argument("For string data "
                "expected STRING buftype, got: "+buftype);
        }

        std::string s = py::str(data);
        xatmibuf buf("STRING", s.size() + 1);
        strcpy(*buf.pp, s.c_str());
        return buf;
    }
    else if (py::isinstance<py::dict>(data))
    {
        if (buftype!="" && buftype!="UBF")
        {
            throw std::invalid_argument("For dict data "
                "expected UBF buftype, got: "+buftype);
        }
        xatmibuf buf("UBF", 1024);

        ndrxpy_from_py_ubf(static_cast<py::dict>(data), buf);

        return buf;
    }
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }
}


/* vim: set ts=4 sw=4 et smartindent: */
