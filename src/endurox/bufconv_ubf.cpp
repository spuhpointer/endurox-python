/**
 * @brief Typed buffer to Python conversion, UBF type
 *
 * @file bufconv_ubf.cpp
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
 * @brief Convert UBF buffer to python object
 * 
 * @param fbfr UBF buffer handler
 * @param buflen buffer len (opt)
 * @return py::object converted object
 */
expublic py::object ndrxpy_to_py_ubf(UBFH *fbfr, BFLDLEN buflen = 0)
{
    BFLDID fieldid = BFIRSTFLDID;
    Bnext_state_t state;
    BFLDOCC oc = 0;
    char *d_ptr;
    BVIEWFLD *p_vf;

    py::dict result;
    py::list val;

    NDRX_LOG(log_debug, "Into ndrxpy_to_py_ubf()");

    if (buflen == 0)
    {
        buflen = Bsizeof(fbfr);
    }
    /*std::unique_ptr<char[]> value(new char[buflen]); */

    Bprint(fbfr);

    for (;;)
    {
        BFLDLEN len = buflen;
        //Seems in Enduro/X state is not associate with particular buffer
        //Looks like Tuxedo stores iteration state within buffer it self.
        int r = Bnext2(&state, fbfr, &fieldid, &oc, NULL, &len, &d_ptr);
        if (r == -1)
        {
            throw ubf_exception(Berror);
        }
        else if (r == 0)
        {
            break;
        }

        if (oc == 0)
        {
            val = py::list();

            char *name = Bfname(fieldid);
            if (name != nullptr)
            {
                result[name] = val;
            }
            else
            {
                result[py::int_(fieldid)] = val;
            }
        }

        switch (Bfldtype(fieldid))
        {
        case BFLD_CHAR:
            /* if EOS char is used, convert to byte array.
             * as it is possible to get this value from C
             */
            if  (EXEOS==d_ptr[0])
            {
                val.append(py::bytes(d_ptr, 1));
            }
            else
            {
                val.append(py::cast(d_ptr[0]));
            }            
            break;
        case BFLD_SHORT:
            val.append(py::cast(*reinterpret_cast<short *>(d_ptr)));
            break;
        case BFLD_LONG:
            val.append(py::cast(*reinterpret_cast<long *>(d_ptr)));
            break;
        case BFLD_FLOAT:
            val.append(py::cast(*reinterpret_cast<float *>(d_ptr)));
            break;
        case BFLD_DOUBLE:
            val.append(py::cast(*reinterpret_cast<double *>(d_ptr)));
            break;
        case BFLD_STRING:

            NDRX_LOG(log_debug, "Processing FLD_STRING... [%s]", d_ptr);
            val.append(
#if PY_MAJOR_VERSION >= 3
                py::str(d_ptr)
                //Seems like this one causes memory leak:
                //Thus assume t
                //py::str(PyUnicode_DecodeLocale(value.get(), "surrogateescape"))
#else
                py::bytes(d_ptr, len - 1)
#endif
            );
            break;
        case BFLD_CARRAY:
            val.append(py::bytes(d_ptr, len));
            break;
        case BFLD_UBF:
            val.append(ndrxpy_to_py_ubf(reinterpret_cast<UBFH *>(d_ptr), buflen));
            break;
        case BFLD_VIEW:
        {
            py::dict vdict;

            /* d_ptr points to BVIEWFIELD */
            p_vf = reinterpret_cast<BVIEWFLD *>(d_ptr);

            if (EXEOS!=p_vf->vname[0])
            {
                /* not empty occ */
                vdict["vname"] = p_vf->vname;
                vdict["data"]= ndrxpy_to_py_view(p_vf->data, p_vf->vname, len);
            }

            val.append(vdict);
            break;
        }
        case BFLD_PTR:
            /* PTR ?  */
        break;
        default:
            throw std::invalid_argument("Unsupported field " +
                                        std::to_string(fieldid));
        }
    }
    return result;
}

/**
 * @brief Build UBF buffer from PY dict
 * 
 * @param buf 
 * @param fieldid 
 * @param oc 
 * @param obj 
 * @param b temporary buffer
 */
static void from_py1_ubf(xatmibuf &buf, BFLDID fieldid, BFLDOCC oc,
                     py::handle obj, xatmibuf &b)
{
    if (obj.is_none())
    {

#if PY_MAJOR_VERSION >= 3
    }
    else if (py::isinstance<py::bytes>(obj))
    {
        std::string val(PyBytes_AsString(obj.ptr()), PyBytes_Size(obj.ptr()));

        buf.mutate([&](UBFH *fbfr)
                   { return CBchg(fbfr, fieldid, oc, const_cast<char *>(val.data()),
                                  val.size(), BFLD_CARRAY); });
#endif
    }
    else if (py::isinstance<py::str>(obj))
    {
        char *ptr_val =NULL;
        BFLDLEN len;

        std::string yopt;
#if PY_MAJOR_VERSION >= 3
        py::bytes b = py::reinterpret_steal<py::bytes>(
            PyUnicode_EncodeLocale(obj.ptr(), "surrogateescape"));

        //If we get NULL ptr, then string contains null characters, and that is not supported
        //In case of string we get EOS
        //In case of single char field, will get NULL field.
        std::string val = "";

        if (nullptr!=b.ptr())
        {
            //TODO: well this goes out of the scope?
            val.assign(PyBytes_AsString(b.ptr()), PyBytes_Size(b.ptr()));
            ptr_val = const_cast<char *>(val.data());
            len = val.size();
        }
        else
        {
            PyErr_Print();
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "Invalid string value probably contains 0x00 (len=%ld), field=%d",
                PyBytes_Size(obj.ptr()), fieldid);
            throw std::invalid_argument(tmp);
        }

#else
        if (PyUnicode_Check(obj.ptr()))
        { 
            obj = PyUnicode_AsEncodedString(obj.ptr(), "utf-8", "surrogateescape");
        }
        std::string val(PyString_AsString(obj.ptr()), PyString_Size(obj.ptr()));

        ptr_val = const_cast<char *>(val.data());
        len = val.size();

#endif
        buf.mutate([&](UBFH *fbfr)
                   { return CBchg(fbfr, fieldid, oc, ptr_val, len, BFLD_CARRAY); });
    }
    else if (py::isinstance<py::int_>(obj))
    {
        long val = obj.cast<py::int_>();
        buf.mutate([&](UBFH *fbfr)
                   { return CBchg(fbfr, fieldid, oc, reinterpret_cast<char *>(&val), 0,
                                  BFLD_LONG); });
    }
    else if (py::isinstance<py::float_>(obj))
    {
        double val = obj.cast<py::float_>();
        buf.mutate([&](UBFH *fbfr)
                   { return CBchg(fbfr, fieldid, oc, reinterpret_cast<char *>(&val), 0,
                                  BFLD_DOUBLE); });
    }
    else if (py::isinstance<py::dict>(obj))
    {
        if (BFLD_UBF==Bfldtype(fieldid))
        {
            ndrxpy_from_py_ubf(obj.cast<py::dict>(), b);
            buf.mutate([&](UBFH *fbfr)
                    { return Bchg(fbfr, fieldid, oc, reinterpret_cast<char *>(*b.fbfr()), 0); });
        }
        else if (BFLD_VIEW==Bfldtype(fieldid))
        {
            /*
             * Syntax: data: { "VIEW_FIELD":{"vname":"VIEW_NAME", "data":{}} } 
             */
            auto view_d = obj.cast<py::dict>();

            auto have_vnamed = view_d.contains("vname");
            auto have_data = view_d.contains("data");
            BVIEWFLD vf;
            memset(&vf, 0, sizeof(vf));

            if (have_vnamed && !have_data)
            {
                //Invalid condition, but must be present
                UBF_LOG(log_debug, "Failed to convert view field %d: vname present but no data", fieldid);
                throw std::invalid_argument("vname present but no data");
            }
            else if (!have_vnamed && have_data)
            {
                //Invalid condition
               UBF_LOG(log_debug, "Failed to convert view field %d: data present but no vname", fieldid);
                throw std::invalid_argument("data present but no vname");
            }
            else if (!have_vnamed && !have_data)
            {
                //put empty..
            }
            else
            {
                auto vnamed = view_d["vname"];
                auto vdata = view_d["data"];
                

                std::string vname = py::str(vnamed);

                NDRX_STRCPY_SAFE(vf.vname, vname.c_str());
                xatmibuf vbuf("VIEW", vf.vname);

                vf.data = *vbuf.pp;
                vf.vflags=0;

                ndrxpy_from_py_view(vdata.cast<py::dict>(), vbuf, vf.vname);
            }

            buf.mutate([&](UBFH *fbfr)
                    { return Bchg(fbfr, fieldid, oc, reinterpret_cast<char *>(&vf), 0); });

        }
    }
    else
    {
        throw std::invalid_argument("Unsupported type");
    }
}

/**
 * @brief Convert PY to UBF
 * 
 * @param obj 
 * @param b 
 */
expublic void ndrxpy_from_py_ubf(py::dict obj, xatmibuf &b)
{
    b.reinit("UBF", nullptr, 1024);
    xatmibuf f;

    for (auto it : obj)
    {
        BFLDID fieldid;
        if (py::isinstance<py::int_>(it.first))
        {
            fieldid = it.first.cast<py::int_>();
        }
        else
        {
            fieldid =
                Bfldid(const_cast<char *>(std::string(py::str(it.first)).c_str()));
        }

        py::handle o = it.second;
        if (py::isinstance<py::list>(o))
        {
            BFLDOCC oc = 0;
            for (auto e : o.cast<py::list>())
            {
                from_py1_ubf(b, fieldid, oc++, e, f);
            }
        }
        else
        {
            // Handle single elements instead of lists for convenience
            from_py1_ubf(b, fieldid, 0, o, f);
        }
    }

    Bprint(*b.fbfr());
}


/* vim: set ts=4 sw=4 et smartindent: */