/**
 * @brief XATMI Server extensions
 *
 * @file tpext.cpp
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
#include <mutex>

/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
/*---------------------------Statics------------------------------------*/

/** current handle for b4poll callback */
ndrxpy_object_t * M_b4pollcb_handler = nullptr;

/** periodic server callback handler */
ndrxpy_object_t * M_addperiodcb_handler = nullptr;

/*---------------------------Prototypes---------------------------------*/

namespace py = pybind11;

/**
 * @brief Dispatch b4 poll callback
 */
exprivate int ndrxpy_b4pollcb_callback(void)
{
    //Get the gil...
    py::gil_scoped_acquire acquire;

    py::object ret = M_b4pollcb_handler->obj();
    return ret.cast<int>();

}

/**
 * @brief register b4 poll callback handler.
 * 
 * @param func callback function
 */
exprivate void ndrxpy_tpext_addb4pollcb (const py::object &func)
{
    //Allocate the object
    if (nullptr!=M_b4pollcb_handler)
    {
        delete M_b4pollcb_handler;
        M_b4pollcb_handler = nullptr;
    }

    M_b4pollcb_handler = new ndrxpy_object_t();
    M_b4pollcb_handler->obj = func;

    if (EXSUCCEED!=tpext_addb4pollcb(ndrxpy_b4pollcb_callback))
    {
        throw xatmi_exception(tperrno);
    }
}


/**
 * @brief Periodic callback dispatch
 */
exprivate int ndrxpy_addperiodcb_callback(void)
{
    //Get the gil...
    py::gil_scoped_acquire acquire;
    py::object ret = M_addperiodcb_handler->obj();
    return ret.cast<int>();
}

/**
 * @brief register periodic callback handler
 * @param secs number of seconds for callback interval
 * @param func callback function
 */
exprivate void ndrxpy_tpext_addperiodcb (int secs, const py::object &func)
{
    //Allocate the object
    if (nullptr!=M_addperiodcb_handler)
    {
        delete M_addperiodcb_handler;
        M_addperiodcb_handler = nullptr;
    }

    M_addperiodcb_handler = new ndrxpy_object_t();
    M_addperiodcb_handler->obj = func;

    if (EXSUCCEED!=tpext_addperiodcb(secs, ndrxpy_addperiodcb_callback))
    {
        throw xatmi_exception(tperrno);
    }
}

/**
 * @brief Register XATMI server extensions
 * 
 * @param m Pybind11 module handle
 */
expublic void ndrxpy_register_tpext(py::module &m)
{
    m.def(
        "tpext_addb4pollcb", [](const py::object &func)
        { ndrxpy_tpext_addb4pollcb(func); },
        "Register callback handler for server going. Work only from server thread. "
        "API is not thread safe. For MT servers only use in tpsvrinit().", py::arg("func"));

    m.def(
        "tpext_delb4pollcb", [](void)
        {   
            if (EXSUCCEED!=tpext_delb4pollcb())
            {
                throw xatmi_exception(tperrno);
            }

            //Reset handler...
            delete M_b4pollcb_handler;
            M_b4pollcb_handler = nullptr;
        },
        "Remove before-poll callback");

     m.def(
        "tpext_addperiodcb", [](int secs, const py::object &func)
        { ndrxpy_tpext_addperiodcb(secs, func); },
        "Register periodic callback handler. API is not thread safe. For MT servers only use in tpsvrinit().", 
        py::arg("secs"), py::arg("func"));

    m.def(
        "tpext_delperiodcb", [](void)
        {   
            if (EXSUCCEED!=tpext_delperiodcb())
            {
                throw xatmi_exception(tperrno);
            }

            //Reset handler...
            delete M_addperiodcb_handler;
            M_addperiodcb_handler = nullptr;
        },
        "Remove before-poll callback. API is not thread safe. For MT servers only use in tpsvrinit().");

/*
tpext_addpollerfd
tpext_delpollerfd
*/
    //TODO
}

/* vim: set ts=4 sw=4 et smartindent: */

