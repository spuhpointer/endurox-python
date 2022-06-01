/**
 * @brief ATMI Server extensions
 *
 * @file tpext.cpp
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

/** filedescriptor map to py callbacks */
std::map<int, ndrxpy_object_t*> M_fdmap {};

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
        throw atmi_exception(tperrno);
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
        throw atmi_exception(tperrno);
    }
}

/**
 * @brief pollevent callback
 * 
 * @param fd monitored file descriptor
 * @param events events monitored
 * @param ptr1 not used
 * @return -1 on failure, 0 ok 
 */
exprivate int ndrxpy_pollevent_cb(int fd, uint32_t events, void *ptr1)
{
    py::gil_scoped_acquire acquire;
    py::object ret=M_fdmap[fd]->obj(fd, events, M_fdmap[fd]->obj2);
    return ret.cast<int>();
}

/**
 * @brief Register extensions callback
 * 
 * @param fd file descriptor
 * @param events poll events
 * @param ptr1 object to pass back
 * @param func callback func
 */
exprivate void ndrxpy_tpext_addpollerfd (int fd, uint32_t events, const py::object ptr1, const py::object &func)
{
    ndrxpy_object_t * obj = new ndrxpy_object_t();

    obj->obj = func;
    obj->obj2 = ptr1;

    if (EXSUCCEED!=tpext_addpollerfd(fd, events, NULL, ndrxpy_pollevent_cb))
    {
        throw atmi_exception(tperrno);
    }

    M_fdmap[fd] = obj;
}

/**
 * @brief remove fd from polling
 * 
 * @param fd file descriptor
 */
exprivate void ndrxpy_tpext_delpollerfd(int fd)
{
    auto it = M_fdmap.find(fd);
    if (it != M_fdmap.end()) {
        delete M_fdmap[fd];
        M_fdmap.erase(it);
    }

    if (EXSUCCEED!=tpext_delpollerfd(fd))
    {
        throw atmi_exception(tperrno);
    }
}

/**
 * @brief Register ATMI server extensions
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
                throw atmi_exception(tperrno);
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
                throw atmi_exception(tperrno);
            }

            //Reset handler...
            delete M_addperiodcb_handler;
            M_addperiodcb_handler = nullptr;
        },
        "Remove before-poll callback. API is not thread safe. For MT servers only use in tpsvrinit().");

     m.def(
        "tpext_addpollerfd", [](int fd, uint32_t events, const py::object ptr1, const py::object &func)
        { ndrxpy_tpext_addpollerfd(fd, events, ptr1, func); },
        "Add file descriptor to atmi server polling", 
        py::arg("fd"), py::arg("events"), py::arg("ptr1"), py::arg("func"));


     m.def(
        "tpext_delpollerfd", [](int fd)
        { ndrxpy_tpext_delpollerfd(fd); },
        "Delete file descriptor from ATMI poller", 
        py::arg("fd"));
}

/* vim: set ts=4 sw=4 et smartindent: */
