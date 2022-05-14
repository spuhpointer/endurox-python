/**
 * @brief Logging API
 *
 * @file tplog.cpp
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
 * @brief Register XATMI logging api
 * 
 * @param m Pybind11 module handle
 */
expublic void ndrxpy_register_tplog(py::module &m)
{

    //Debug handle
    py::class_<pyndrxdebugptr>(m, "NdrxDebugHandle")
        //this is buffer for pointer...
        .def_readonly("ptr", &pyndrxdebugptr::ptr);

    //Logging functions:
    m.def(
        "tplog_debug",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_debug, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_info",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_info, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_warn",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        "Print warning log message", py::arg("message"));

    m.def(
        "tplog_error",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        "Print error log message", py::arg("message"));

    m.def(
        "tplog_always",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        "Print fatal log message", py::arg("message"));

    m.def(
        "tplog",
        [](int lev, const char *message)
        {
            py::gil_scoped_release release;
            tplog(lev, const_cast<char *>(message));
        },
        "Print log message with specified level", py::arg("lev"), py::arg("message"));

    m.def(
        "tplogconfig",
        [](int logger, int lev, const char *debug_string, const char *module, const char *new_file)
        {
            py::gil_scoped_release release;
            if (EXSUCCEED!=tplogconfig(logger, lev, const_cast<char *>(debug_string), 
                const_cast<char *>(module), const_cast<char *>(new_file)))
            {
                throw xatmi_exception(tperrno);
            }
        },
        "Configure logger", py::arg("logger"), py::arg("lev"), 
        py::arg("debug_string"), py::arg("module"), py::arg("new_file"));

    m.def(
        "tplogqinfo",
        [](int lev, long flags)
        {
            py::gil_scoped_release release;
            long ret=tplogqinfo(lev,flags);
            if (EXFAIL==ret)
            {
                throw xatmi_exception(tperrno); 
            }

            return ret;
        },
        "Get logger info", py::arg("lev"), py::arg("flags"));

    m.def(
        "tplogsetreqfile",
        [](py::object data, const char * filename, const char * filesvc)
        {

            xatmibuf in;

            if (!py::isinstance<py::none>(data))
            {
                in = ndrx_from_py(data);
            }
            
            {
                char type[8]={EXEOS};
                char subtype[16]={EXEOS};
                py::gil_scoped_release release;

                //Check is it UBF or not?

                if (tptypes(*in.pp, type, subtype) == EXFAIL)
                {
                    NDRX_LOG(log_error, "Invalid buffer type");
                    throw std::invalid_argument("Invalid buffer type");
                }

                if (EXFAIL==tplogsetreqfile( (0==strcmp(type, "UBF")?in.pp:NULL), const_cast<char *>(filename), 
                    const_cast<char *>(filesvc)))
                {
                    // In case if buffer changed..
                    in.p=*in.pp;
                    throw xatmi_exception(tperrno);   
                }
                // In case if buffer changed..
                in.p=*in.pp;
            }

            //Return python object... (in case if one was passed in...)
            return ndrx_to_py(in);
        },
        "Redirect logger to request file extracted from buffer, filename or file name service", 
            py::arg("data"), py::arg("filename")="", py::arg("filesvc")="");

    m.def(
        "tplogsetreqfile_direct",
        [](std::string filename)
        {
            py::gil_scoped_release release;
            tplogsetreqfile_direct(const_cast<char *>(filename.c_str()));
        },
        "Set request log file from filename only", 
            py::arg("filename")="");

    m.def(
        "tploggetbufreqfile",
        [](py::object data)
        {
            char filename[PATH_MAX+1];
            auto in = ndrx_from_py(data);
            {
                py::gil_scoped_release release;
                if (EXSUCCEED!=tploggetbufreqfile(*in.pp, filename, sizeof(filename)))
                {
                    throw xatmi_exception(tperrno); 
                }
            }
            return py::str(filename);
        },
        "Get request file name from UBF buffer",
        py::arg("data"));

     m.def(
        "tploggetreqfile",
        [](void)
        {
            char filename[PATH_MAX+1]="";
            {
                py::gil_scoped_release release;
                tploggetreqfile(filename, sizeof(filename));
            }
            return py::str(filename);
        },
        "Get current request log file, returns empty if one is not set");

     m.def(
        "tplogdelbufreqfile",
        [](py::object data)
        {
            auto in = ndrx_from_py(data);
            {
                py::gil_scoped_release release;
                if (EXSUCCEED!=tplogdelbufreqfile(*in.pp))
                {
                        throw xatmi_exception(tperrno);
                }
            }

            return ndrx_to_py(in);
        },
        "Get current request log file, returns empty if one is not set",
        py::arg("data"));

     m.def(
        "tplogclosereqfile",
        [](void)
        {
            py::gil_scoped_release release;
            tplogclosereqfile();
        },
        "Close request logging file (if one is currenlty open)");

     m.def(
        "tplogclosethread",
        [](void)
        {
            py::gil_scoped_release release;
            tplogclosethread();
        },
        "Close tread log file");

     m.def(
        "tplogdump",
        [](int lev, const char * comment, py::bytes data)
        {
            std::string val(PyBytes_AsString(data.ptr()), PyBytes_Size(data.ptr()));

            py::gil_scoped_release release;
            tplogdump(lev, const_cast<char *>(comment), 
                const_cast<char *>(val.data()), val.size());
        },
        "Produce hex dump of byte array",
        py::arg("lev"), py::arg("comment"), py::arg("data"));

     m.def(
        "tplogdumpdiff",
        [](int lev, const char * comment, py::bytes data1, py::bytes data2)
        {
            std::string val1(PyBytes_AsString(data1.ptr()), PyBytes_Size(data1.ptr()));
            std::string val2(PyBytes_AsString(data2.ptr()), PyBytes_Size(data2.ptr()));

            long len = std::min(val1.size(), val2.size());

            py::gil_scoped_release release;
            tplogdumpdiff(lev, const_cast<char *>(comment), 
                const_cast<char *>(val1.data()), const_cast<char *>(val2.data()), 
                len);
        },
        "Compare two byte arrays and print differences in the log"
        ,
        py::arg("lev"), py::arg("comment"), py::arg("data1"), py::arg("data2"));

     m.def(
        "tplogfplock",
        [](int lev, long flags)
        {
            return pyndrxdebugptr(tplogfplock(lev, flags));
        },
        "Lock the file pointer / fileno"
        ,
        py::arg("lev")=-1, py::arg("flags")=0);

     m.def(
        "tplogfpget",
        [](pyndrxdebugptr dbg, long flags)
        {
            ndrx_debug_t *ptr = reinterpret_cast<ndrx_debug_t *>(dbg.ptr);
            return fileno(tplogfpget(ptr, flags));
        },
        "Get file descriptor for Python."
        ,
        py::arg("dbg"), py::arg("flags")=0);

     m.def(
        "tplogfpunlock",
        [](pyndrxdebugptr dbg)
        {
            ndrx_debug_t *ptr = reinterpret_cast<ndrx_debug_t *>(dbg.ptr);
            tplogfpunlock(ptr);
        },
        "Unlock the debug handle"
        ,
        py::arg("dbg"));

     m.def(
        "tplogprintubf",
        [](int lev, const char *title, py::object data)
        {
            auto in = ndrx_from_py(data);
            py::gil_scoped_release release;
            tplogprintubf(lev, const_cast<char *>(title), reinterpret_cast<UBFH *>(*in.pp));
        },
        "Get file descriptor for Python."
        ,
        py::arg("lev"), py::arg("title"), py::arg("data"));

}

/* vim: set ts=4 sw=4 et smartindent: */

