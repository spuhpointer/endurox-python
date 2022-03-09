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

namespace py = pybind11;

static void from_py(py::dict obj, xatmibuf &b);

static py::object to_py(UBFH *fbfr, BFLDLEN buflen = 0)
{
    BFLDID fieldid = BFIRSTFLDID;
    Bnext_state_t state;
    BFLDOCC oc = 0;
    char *d_ptr;

    py::dict result;
    py::list val;

    if (buflen == 0)
    {
        buflen = Bsizeof(fbfr);
    }
    /*std::unique_ptr<char[]> value(new char[buflen]); */

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
            val.append(py::cast(d_ptr[0]));
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
            val.append(to_py(reinterpret_cast<UBFH *>(d_ptr), buflen));
            break;
        default:
            throw std::invalid_argument("Unsupported field " +
                                        std::to_string(fieldid));
        }
    }
    return result;
}

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

    if (tptypes(*buf.pp, type, subtype) == -1)
    {
        throw std::invalid_argument("Invalid buffer type");
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
        result["data"]=to_py(*buf.fbfr());
    }
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }

    return result;
}

static void from_py1(xatmibuf &buf, BFLDID fieldid, BFLDOCC oc,
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
        buf.mutate([&](UBFH *fbfr)
                   { return CBchg(fbfr, fieldid, oc, const_cast<char *>(val.data()),
                                  val.size(), BFLD_CARRAY); });
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
        from_py(obj.cast<py::dict>(), b);
        buf.mutate([&](UBFH *fbfr)
                   { return Bchg(fbfr, fieldid, oc, reinterpret_cast<char *>(*b.fbfr()), 0); });
    }
    else
    {
        throw std::invalid_argument("Unsupported type");
    }
}

static void from_py(py::dict obj, xatmibuf &b)
{
    b.reinit("UBF", 1024);
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
                from_py1(b, fieldid, oc++, e, f);
            }
        }
        else
        {
            // Handle single elements instead of lists for convenience
            from_py1(b, fieldid, 0, o, f);
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

    if (!py::isinstance<py::dict>(obj))
    {
        throw std::invalid_argument("Unsupported buffer type");
    }
    //Get the data field

    auto dict = static_cast<py::dict>(obj);

    //data i
    auto data = dict["data"];

    if (py::isinstance<py::bytes>(data))
    {
        xatmibuf buf("CARRAY", PyBytes_Size(data.ptr()));
        memcpy(*buf.pp, PyBytes_AsString(data.ptr()), PyBytes_Size(data.ptr()));
        return buf;
    }
    else if (py::isinstance<py::str>(data))
    {
        std::string s = py::str(data);
        xatmibuf buf("STRING", s.size() + 1);
        strcpy(*buf.pp, s.c_str());
        return buf;
    }
    else if (py::isinstance<py::dict>(data))
    {
        xatmibuf buf("UBF", 1024);

        from_py(static_cast<py::dict>(data), buf);

        return buf;
    }
    else
    {
        throw std::invalid_argument("Unsupported buffer type");
    }
}
