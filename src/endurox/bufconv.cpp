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
 *  TODO: Free incoming UBF buffer (somehere marking shall be put)
 * @param buf XATMI buffer to conver to Python
 * @param is_master is master buffer (from xatmi?)
 * @return python object (dict)
 */
expublic py::object ndrx_to_py(xatmibuf buf, bool is_master)
{
    char type[8]={EXEOS};
    char subtype[16]={EXEOS};
    long size;
    py::dict result;
    int ret;

    if ((size=tptypes(*buf.pp, type, subtype)) == EXFAIL)
    {
        NDRX_LOG(log_error, "Invalid buffer type");
        throw std::invalid_argument("Invalid buffer type");
    }

    NDRX_LOG(log_debug, "Into ndrx_to_py() type=[%s] subtype=[%s] size=%ld", 
        type, subtype, size);

    //Return buffer sub-type
    result["buftype"] = type;

    if (EXEOS!=subtype[0])
    {
        result["subtype"]=subtype;
    }

    NDRX_LOG(log_debug, "Converting buffer type [%s]", type);

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

        //Free up as master, recursive
        if (is_master)
        {
            buf.do_free_ptrs=NDRXPY_DO_FREE;
        }
    }
    else if (strcmp(type, "VIEW") == 0)
    {
        result["data"] = ndrxpy_to_py_view(*buf.pp, subtype, size);
    }
    else if (strcmp(type, "NULL") == 0)
    {
        /* data field not present -> NULL */
    } 
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }

    // attach call info, if have any.
    xatmibuf cibuf;
    if (strcmp(type, "NULL") != 0)
    {
        if (EXSUCCEED==tpgetcallinfo(*buf.pp, reinterpret_cast<UBFH **>(cibuf.pp), 0))
        {
            // setup callinfo block
            result[NDRXPY_DATA_CALLINFO]=ndrxpy_to_py_ubf(*cibuf.fbfr(), 0);
        }
        else if (TPESYSTEM!=tperrno)
        {
            NDRX_LOG(log_debug, "Error checking tpgetcallinfo()");
            throw xatmi_exception(tperrno);
        }
    }

    return result;
}

/**
 * @brief Process call info from main call dict
 * 
 * @param dict dictionary used for call
 * @param buf prepared ATMI buffer
 */
exprivate void set_callinfo(py::dict & dict, xatmibuf &buf)
{
    if (dict.contains(NDRXPY_DATA_CALLINFO))
    {
        xatmibuf cibuf;
        auto cibufdata = dict[NDRXPY_DATA_CALLINFO];

        NDRX_LOG(log_debug, "Setting call info");

        if (!py::isinstance<py::dict>(cibufdata))
        {
            NDRX_LOG(log_error, "callinfo must be dictionary but is not!");
            throw std::invalid_argument("callinfo must be dictionary but is not!");
        }

        if (NULL==*buf.pp)
        {
            NDRX_LOG(log_error, "callinfo cannot be set for NULL buffers!");
            throw std::invalid_argument("callinfo cannot be set for NULL buffers");
        }

        ndrxpy_from_py_ubf(static_cast<py::dict>(cibufdata), cibuf);

        if (EXSUCCEED!=tpsetcallinfo(*buf.pp, *cibuf.fbfr(), 0))
        {
            throw xatmi_exception(tperrno);
        }
    }
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
    xatmibuf buf;

    NDRX_LOG(log_debug, "Into ndrx_from_py()");

    if (!py::isinstance<py::dict>(obj))
    {
        throw std::invalid_argument("Unsupported buffer type");
    }
    //Get the data field

    auto dict = static_cast<py::dict>(obj);

    //for NULL buffers, we do not contain this...
    py::dict data;
    
    if (dict.contains(NDRXPY_DATA_DATA))
    {
        data = dict[NDRXPY_DATA_DATA];
    }

    if (dict.contains(NDRXPY_DATA_BUFTYPE))
    {
        buftype = py::str(dict[NDRXPY_DATA_BUFTYPE]);
    }
    
    if (dict.contains(NDRXPY_DATA_SUBTYPE))
    {
        subtype = py::str(dict[NDRXPY_DATA_SUBTYPE]);
    }

    NDRX_LOG(log_debug, "Converting out: [%s] / [%s]", buftype.c_str(), subtype.c_str());

    /* process JSON data... as string */
    if (buftype=="JSON")
    {
        if (py::isinstance<py::str>(data))
        {
            throw std::invalid_argument("String expected for JSON buftype, got: "+buftype);
        } 

        std::string s = py::str(data);

        buf = xatmibuf("JSON", s.size() + 1);
        strcpy(*buf.pp, s.c_str());
    }
    else if (buftype=="VIEW")
    {
        if (subtype=="")
        {
            throw std::invalid_argument("subtype expected for VIEW buffer");
        }

        buf = xatmibuf("VIEW", subtype.c_str());

        ndrxpy_from_py_view(static_cast<py::dict>(data), buf, subtype.c_str());
    }
    else if (py::isinstance<py::bytes>(data))
    {
        if (buftype!="" && buftype!="CARRAY")
        {
            throw std::invalid_argument("For byte array data "
                "expected CARRAY buftype, got: "+buftype);
        }
        
        buf = xatmibuf("CARRAY", PyBytes_Size(data.ptr()));
        memcpy(*buf.pp, PyBytes_AsString(data.ptr()), PyBytes_Size(data.ptr()));
    }
    else if (py::isinstance<py::str>(data))
    {
        if (buftype!="" && buftype!="STRING")
        {
            throw std::invalid_argument("For string data "
                "expected STRING buftype, got: "+buftype);
        }

        std::string s = py::str(data);
        buf = xatmibuf("STRING", s.size() + 1);
        strcpy(*buf.pp, s.c_str());
    }
    else if (!dict.contains(NDRXPY_DATA_DATA))
    {
        NDRX_LOG(log_debug, "Converting out NULL buffer");
        buf = xatmibuf("NULL", 1024);
    }
    else if (py::isinstance<py::dict>(data))
    {
        NDRX_LOG(log_debug, "Converting out UBF dict...");

        if (buftype!="" && buftype!="UBF")
        {
            NDRX_LOG(log_error, "For dict data "
                "expected UBF buftype, got [%s]", buftype);

            throw std::invalid_argument("For dict data "
                "expected UBF buftype, got: "+buftype);
        }
        buf = xatmibuf("UBF", 1024);
        NDRX_LOG(log_error, "YOPT FREE %p %d", buf.p, buf.do_free_ptrs);
        ndrxpy_from_py_ubf(static_cast<py::dict>(data), buf);
    }
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }

    set_callinfo(dict, buf);

    return buf;
}


/* vim: set ts=4 sw=4 et smartindent: */
