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
    //Logging functions:
    m.def(
        "tplog_debug",
        [](const char *message)
        {
            tplog(log_debug, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_info",
        [](const char *message)
        {
            tplog(log_info, const_cast<char *>(message));
        },
        "Print debug log message", py::arg("message"));

    m.def(
        "tplog_warn",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print warning log message", py::arg("message"));

    m.def(
        "tplog_error",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print error log message", py::arg("message"));

    m.def(
        "tplog_always",
        [](const char *message)
        {
            tplog(log_error, const_cast<char *>(message));
        },
        "Print fatal log message", py::arg("message"));

    m.def(
        "tplog",
        [](int lev, const char *message)
        {
            tplog(lev, const_cast<char *>(message));
        },
        "Print log message with specified level", py::arg("lev"), py::arg("message"));

    m.def(
        "tplogconfig",
        [](int logger, int lev, const char *debug_string, const char *module, const char *new_file)
        {
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
        [](py::object data, std::string filename, std::string filesvc)
        {

            auto in = ndrx_from_py(data);

            if (EXFAIL==tplogsetreqfile(in.pp, const_cast<char *>(filename.c_str()), 
                const_cast<char *>(filesvc.c_str())))
            {
                // In case if buffer changed..
                in.p=*in.pp;
                throw xatmi_exception(tperrno);   
            }
            // In case if buffer changed..
            in.p=*in.pp;
        },
        "Redirect logger to request file extracted from buffer, filename or file name service", 
            py::arg("data"), py::arg("filename")="", py::arg("filesvc")="");

    m.def(
        "tplogsetreqfile_direct",
        [](std::string filename)
        {
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
            if (EXSUCCEED!=tploggetbufreqfile(*in.pp, filename, sizeof(filename)))
            {
                throw xatmi_exception(tperrno); 
            }
            return py::str(filename);
        },
        "Get request file name from UBF buffer");

     m.def(
        "tploggetreqfile",
        [](void)
        {
            char filename[PATH_MAX+1]="";
            tploggetreqfile(filename, sizeof(filename));
            return py::str(filename);
        },
        "Get current request log file, returns empty if one is not set");

     m.def(
        "tplogdelbufreqfile",
        [](py::object data)
        {
           auto in = ndrx_from_py(data);

           if (EXSUCCEED!=tplogdelbufreqfile(*in.pp))
           {
                throw xatmi_exception(tperrno);
           }

            return ndrx_to_py(in);
        },
        "Get current request log file, returns empty if one is not set");

     m.def(
        "tplogclosereqfile",
        [](void)
        {
           tplogclosereqfile();
        },
        "Close request logging file (if one is currenlty open)");

     m.def(
        "tplogclosethread",
        [](void)
        {
           tplogclosethread();
        },
        "Close tread log file");

        /*

TODO:

tplogdump.3
tplogdumpdiff.3
tplogfpget.3
tplogfplock.3
tplogfpunlock.3

tplogprintubf.3

*/

}


/* vim: set ts=4 sw=4 et smartindent: */

