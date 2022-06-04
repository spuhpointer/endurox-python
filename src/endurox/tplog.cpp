/**
 * @brief Logging API
 *
 * @file tplog.cpp
 */
/* -----------------------------------------------------------------------------
 * Python module for Enduro/X
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
#include <nerror.h>
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
 * @brief Register ATMI logging api
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
        R"pbdoc(
        Print debug message to log file. Debug is logged as level **5**.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        message : str
            Debug message to print.
        )pbdoc"
        , py::arg("message"));

    m.def(
        "tplog_info",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_info, const_cast<char *>(message));
        },
        R"pbdoc(
        Print info message to log file. Info is logged as level **4**.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        message : str
            Info message to print.
        )pbdoc"
        , py::arg("message"));

    m.def(
        "tplog_warn",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        R"pbdoc(
        Print warning message to log file. Info is logged as level **3**.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        message : str
            Warning message to print.
        )pbdoc", py::arg("message"));

    m.def(
        "tplog_error",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        R"pbdoc(
        Print error message to log file. Info is logged as level **2**.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        message : str
            Error message to print.
        )pbdoc", py::arg("message"));

    m.def(
        "tplog_always",
        [](const char *message)
        {
            py::gil_scoped_release release;
            tplog(log_error, const_cast<char *>(message));
        },
        R"pbdoc(
        Print fatal message to log file. Fatal/always is logged as level **1**.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        message : str
            Fatal message to print.
        )pbdoc", py::arg("message"));

    m.def(
        "tplog",
        [](int lev, const char *message)
        {
            py::gil_scoped_release release;
            tplog(lev, const_cast<char *>(message));
        },
        R"pbdoc(
        Print logfile message with specified level.

        For more details see **tplog(3)** C API call

        Parameters
        ----------
        lev : int
            Log level with consts: :data:`.log_dump`, :data:`.log_debug`,
            :data:`.log_info`, :data:`.log_warn`, :data:`.log_error`, :data:`.log_always`
            or specify the number (1..6).
        message : str
            Message to log.
        )pbdoc", py::arg("lev"), py::arg("message"));

    m.def(
        "tplogconfig",
        [](int logger, int lev, const char *debug_string, const char *module, const char *new_file)
        {
            py::gil_scoped_release release;
            if (EXSUCCEED!=tplogconfig(logger, lev, const_cast<char *>(debug_string), 
                const_cast<char *>(module), const_cast<char *>(new_file)))
            {
                throw nstd_exception(Nerror);
            }
        },
        R"pbdoc(
        Configure Enduro/X logger.

        .. code-block:: python
            :caption: tplogconfig example
            :name: tplogconfig-example

                import endurox as e
                e.tplogconfig(e.LOG_FACILITY_TP, -1, "tp=4", "", "/dev/stdout")
                e.tplog_info("Test")
                e.tplog_debug("Test Debug")
                # would print to stdout:
                # t:USER:4:d190fd96:29754:7f35f54f2740:000:20220601:160215386056:tplog       :/tplog.c:0582:Test
                
        For more details see **tplogconfig(3)** C API call.

        :raise NstdException: 
            | Following error codes may be present:
            | **NEFORMAT** - Debug string format error.
            | **NESYSTEM** - System error.

        Parameters
        ----------
        logger : int
            Bitwise flags for logger/topic identification:
            :data:`.LOG_FACILITY_NDRX`, :data:`.LOG_FACILITY_UBF`, :data:`.LOG_FACILITY_TP`
            :data:`.LOG_FACILITY_TP_THREAD`, :data:`.LOG_FACILITY_TP_REQUEST`, :data:`.LOG_FACILITY_NDRX_THREAD`
            :data:`.LOG_FACILITY_UBF_THREAD`, :data:`.LOG_FACILITY_NDRX_REQUEST`, :data:`.LOG_FACILITY_UBF_REQUEST`
        lev: int
            Level to set. Use **-1** to ignore this setting.
        debug_string : str
            Debug string according to **ndrxdebug.conf(5)**.
        module : str
            Module name. Use empty string if not changing.
        new_file : str
            New logfile name. Use empty string if not changing.

         )pbdoc", py::arg("logger"), py::arg("lev"), 
        py::arg("debug_string"), py::arg("module"), py::arg("new_file"));

    m.def(
        "tplogqinfo",
        [](int lev, long flags)
        {
            py::gil_scoped_release release;
            long ret=tplogqinfo(lev,flags);
            if (EXFAIL==ret)
            {
                throw nstd_exception(Nerror); 
            }

            return ret;
        },
        R"pbdoc(
        Query logger information.

        .. code-block:: python
            :caption: tplogqinfo example
            :name: tplogqinfo-example

                import endurox as e
                e.tplogconfig(e.LOG_FACILITY_TP, -1, "tp=4", "", "/dev/stdout")
                info = e.tplogqinfo(4, e.TPLOGQI_GET_TP)

                if info & 0x0000ffff == e.LOG_FACILITY_TP:
                    e.tplog_info("LOG_FACILITY_TP Matched")
                
                if info >> 24 == 4:
                    e.tplog_info("Log level 4 matched")

                info = e.tplogqinfo(5, e.TPLOGQI_GET_TP)

                if info == 0:
                    e.tplog_info("Not logging level 5")

                # Above would print:
                # t:USER:4:c9e5ad48:23764:7fc434fdd740:000:20220603:232144217247:tplog       :/tplog.c:0582:LOG_FACILITY_TP Matched
                # t:USER:4:c9e5ad48:23764:7fc434fdd740:000:20220603:232144217275:tplog       :/tplog.c:0582:Log level 4 matched
                # t:USER:4:c9e5ad48:23764:7fc434fdd740:000:20220603:232144217288:tplog       :/tplog.c:0582:Not logging level 5
                
        For more details see **tplogqinfo(3)** C API call.

        :raise NstdException: 
            | Following error codes may be present:
            | **NEINVAL** - Invalid logger flag.

        Parameters
        ----------
        lev: int
            Check request log level. If given level is higher than configured, value **0** is returned
            by the function. Unless :data:`.TPLOGQI_EVAL_RETURN` flag is passed, in such case lev does not
            affect the result of the function.
        flags : int
            One of the following flag (exclusive) :data:`.TPLOGQI_GET_NDRX`, :data:`TPLOGQI_GET_UBF` or 
            :data:`.TPLOGQI_GET_TP`. The previous flag may be bitwise or'd with :data:`.TPLOGQI_EVAL_RETURN`
            and :data:`.TPLOGQI_EVAL_DETAILED`

        Returns
        -------
        ret : int
            Bit flags of currently active logger (one of) according to input *flags*:
            :data:`.LOG_FACILITY_NDRX`, :data:`.LOG_FACILITY_UBF`, :data:`.LOG_FACILITY_TP`, 
            :data:`.LOG_FACILITY_TP_THREAD`, :data:`.LOG_FACILITY_TP_REQUEST`, :data:`.LOG_FACILITY_NDRX_THREAD`,
            :data:`.LOG_FACILITY_UBF_THREAD`, :data:`.LOG_FACILITY_NDRX_REQUEST`, :data:`.LOG_FACILITY_UBF_REQUEST`.
            Additionally flag :data:`.TPLOGQI_RET_HAVDETAILED` may be present.
            Byte 4 in return value (according to mask **0xff000000**) contains currently active log level.
            The return value *ret* may be set to **0** in case if *lev* argument indicated higher log level
            than currently used and *flags* did not contain flag :data:`.TPLOGQI_EVAL_RETURN`.  

         )pbdoc", py::arg("lev"), py::arg("flags"));

    m.def(
        "tplogsetreqfile",
        [](py::object data, const char * filename, const char * filesvc)
        {

            atmibuf in;

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
                    throw atmi_exception(tperrno);   
                }
                // In case if buffer changed..
                in.p=*in.pp;
            }

            //Return python object... (in case if one was passed in...)
            return ndrx_to_py(in);
        },
        R"pbdoc(
        Redirect logger to request file extracted from buffer, filename or file name service.
        Note that in case if :data:`.TPESVCFAIL` error is received, exception is thrown
        and if *data* buffer was modified by the service, modification is lost.
        The new log file name must be present in one of the arguments. For full details
        see the C API call.

        .. code-block:: python
            :caption: tplogsetreqfile example
            :name: tplogsetreqfile-example

                import endurox as e
                e.tplogsetreqfile(None, "/tmp/req_1111", None)
                # would print to "/tmp/req_1111"
                e.tplog_info("OK")
                e.tplogclosereqfile()
                
        For more details see **tplogsetreqfile(3)** C API call.

        :raise AtmiException: 
            | Following error codes may be present:
            | **TPEINVAL** - Invalid parameters.
            | **TPENOENT** - *filesvc* is not available.
            | **TPETIME** - *filesvc* timed out.
            | **TPESVCFAIL** - *filesvc* failed.
            | **TPESVCERR**- *filesvc* crashed.
            | **TPESYSTEM** - System error.
            | **TPEOS** - Operating system error.

        Parameters
        ----------
        data: dict
            UBF buffer, where to search for **EX_NREQLOGFILE** field. Or if field is not found
            this buffer is used to call *filesvc*. Parameter is conditional.May use :data:`None` or 
            empty string if not present.
        filename : str
            New request file name. Parameter is conditional. May use :data:`None` or 
            empty string if not present.
        filesvc : str
            Service name to request. Parameter is conditional. And can use :data:`None`
            or empty string if not present.

        Returns
        -------
        ret : dict
            XATMI buffer passed in as *data* argument and/or used for service call.
            If not buffer is used, **NULL** buffer is returned.

         )pbdoc",
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
                    throw atmi_exception(tperrno); 
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
                        throw atmi_exception(tperrno);
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

