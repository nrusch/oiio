// OpenImageIO Copyright 2008- Contributors to the OpenImageIO project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

#include "py_oiio.h"

namespace PyOpenImageIO {


const char*
python_array_code(TypeDesc format)
{
    switch (format.basetype) {
    case TypeDesc::UINT8: return "B";
    case TypeDesc::INT8: return "b";
    case TypeDesc::UINT16: return "H";
    case TypeDesc::INT16: return "h";
    case TypeDesc::UINT32: return "I";
    case TypeDesc::INT32: return "i";
    case TypeDesc::FLOAT: return "f";
    case TypeDesc::DOUBLE: return "d";
    case TypeDesc::HALF: return "e";
    default:
        // For any other type, including UNKNOWN, pack it into an
        // unsigned byte array.
        return "B";
    }
}



TypeDesc
typedesc_from_python_array_code(char code)
{
    switch (code) {
    case 'b':
    case 'c': return TypeDesc::INT8;
    case 'B': return TypeDesc::UINT8;
    case 'h': return TypeDesc::INT16;
    case 'H': return TypeDesc::UINT16;
    case 'i': return TypeDesc::INT;
    case 'I': return TypeDesc::UINT;
    case 'l': return TypeDesc::INT64;
    case 'L': return TypeDesc::UINT64;
    case 'f': return TypeDesc::FLOAT;
    case 'd': return TypeDesc::DOUBLE;
    case 'e': return TypeDesc::HALF;
    }
    return TypeDesc::UNKNOWN;
}



std::string
object_classname(const py::object& obj)
{
    return obj.attr("__class__").attr("__name__").cast<py::str>();
}


oiio_bufinfo::oiio_bufinfo(const py::buffer_info& pybuf, int nchans, int width,
                           int height, int depth, int pixeldims)
{
    if (pybuf.format.size())
        format = typedesc_from_python_array_code(pybuf.format[0]);
    if (size_t(pybuf.itemsize) != format.size()
        || pybuf.size
               != int64_t(width) * int64_t(height) * int64_t(depth * nchans)) {
        format = TypeUnknown;  // Something went wrong
        error  = Strutil::sprintf(
            "buffer is wrong size (expected %dx%dx%dx%d, got total %d)", depth,
            height, width, nchans, pybuf.size);
        return;
    }
    size = pybuf.size;
    if (pixeldims == 3) {
        // Reading a 3D volumetric cube
        if (pybuf.ndim == 4 && pybuf.shape[0] == depth
            && pybuf.shape[1] == height && pybuf.shape[2] == width
            && pybuf.shape[3] == nchans) {
            // passed from python as [z][y][x][c]
            xstride = pybuf.strides[2];
            ystride = pybuf.strides[1];
            zstride = pybuf.strides[0];
        } else if (pybuf.ndim == 3 && pybuf.shape[0] == depth
                   && pybuf.shape[1] == height
                   && pybuf.shape[2] == width * nchans) {
            // passed from python as [z][y][xpixel] -- chans mushed together
            xstride = pybuf.strides[2];
            ystride = pybuf.strides[1];
            zstride = pybuf.strides[0];
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = "Bad dimensions of pixel data";
        }
    } else if (pixeldims == 2) {
        // Reading an 2D image rectangle
        if (pybuf.ndim == 3 && pybuf.shape[0] == height
            && pybuf.shape[1] == width && pybuf.shape[2] == nchans) {
            // passed from python as [y][x][c]
            xstride = pybuf.strides[1];
            ystride = pybuf.strides[0];
        } else if (pybuf.ndim == 2) {
            // Somebody collapsed a dimsision. Is it [pixel][c] with x&y
            // combined, or is it [y][xpixel] with channels mushed together?
            if (pybuf.shape[0] == width * height && pybuf.shape[1] == nchans)
                xstride = pybuf.strides[0];
            else if (pybuf.shape[0] == height
                     && pybuf.shape[1] == width * nchans) {
                ystride = pybuf.strides[0];
                xstride = pybuf.strides[0] * nchans;
            } else {
                format = TypeUnknown;  // error
                error  = Strutil::sprintf(
                    "Can't figure out array shape (pixeldims=%d, pydim=%d)",
                    pixeldims, pybuf.ndim);
            }
        } else if (pybuf.ndim == 1
                   && pybuf.shape[0] == height * width * nchans) {
            // all pixels & channels smushed together
            // just rely on autostride
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = Strutil::sprintf(
                "Can't figure out array shape (pixeldims=%d, pydim=%d)",
                pixeldims, pybuf.ndim);
        }
    } else if (pixeldims == 1) {
        // Reading a 1D scanline span
        if (pybuf.ndim == 2 && pybuf.shape[0] == width
            && pybuf.shape[1] == nchans) {
            // passed from python as [x][c]
            xstride = pybuf.strides[0];
        } else if (pybuf.ndim == 1 && pybuf.shape[0] == width * nchans) {
            // all pixels & channels smushed together
            xstride = pybuf.strides[0] * nchans;
        } else {
            format = TypeUnknown;  // No idea what's going on -- error
            error  = Strutil::sprintf(
                "Can't figure out array shape (pixeldims=%d, pydim=%d)",
                pixeldims, pybuf.ndim);
        }
    } else {
        error = Strutil::sprintf(
            "Can't figure out array shape (pixeldims=%d, pydim=%d)", pixeldims,
            pybuf.ndim);
    }

    if (nchans > 1 && size_t(pybuf.strides.back()) != format.size()) {
        format = TypeUnknown;  // can't handle noncontig channels
        error  = "Can't handle numpy array with noncontiguous channels";
    }
    if (format != TypeUnknown)
        data = pybuf.ptr;
}



bool
oiio_attribute_typed(const std::string& name, TypeDesc type,
                     const py::tuple& obj)
{
    if (type.basetype == TypeDesc::INT) {
        std::vector<int> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() == type.numelements() * type.aggregate)
            return OIIO::attribute(name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::FLOAT) {
        std::vector<float> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() == type.numelements() * type.aggregate)
            return OIIO::attribute(name, type, &vals[0]);
        return false;
    }
    if (type.basetype == TypeDesc::STRING) {
        std::vector<std::string> vals;
        py_to_stdvector(vals, obj);
        if (vals.size() == type.numelements() * type.aggregate) {
            std::vector<ustring> u;
            for (auto& val : vals)
                u.emplace_back(val);
            return OIIO::attribute(name, type, &u[0]);
        }
        return false;
    }
    return false;
}



static py::object
oiio_getattribute_typed(const std::string& name, TypeDesc type = TypeUnknown)
{
    if (type == TypeDesc::UNKNOWN)
        return py::none();
    char* data = OIIO_ALLOCA(char, type.size());
    if (!OIIO::getattribute(name, type, data))
        return py::none();
    if (type.basetype == TypeDesc::INT)
        return C_to_val_or_tuple((const int*)data, type);
    if (type.basetype == TypeDesc::FLOAT)
        return C_to_val_or_tuple((const float*)data, type);
    if (type.basetype == TypeDesc::STRING)
        return C_to_val_or_tuple((const char**)data, type);
    return py::none();
}



// This OIIO_DECLARE_PYMODULE mojo is necessary if we want to pass in the
// MODULE name as a #define. Google for Argument-Prescan for additional
// info on why this is necessary

#define OIIO_DECLARE_PYMODULE(x) PYBIND11_MODULE(x, m)

OIIO_DECLARE_PYMODULE(OIIO_PYMODULE_NAME)
{
    // Basic helper classes
    declare_typedesc(m);
    declare_paramvalue(m);
    declare_imagespec(m);
    declare_roi(m);
    declare_deepdata(m);
    declare_colorconfig(m);

    // Main OIIO I/O classes
    declare_imageinput(m);
    declare_imageoutput(m);
    declare_imagebuf(m);
    declare_imagecache(m);

    declare_imagebufalgo(m);

    // Global (OpenImageIO scope) functions and symbols
    m.def("geterror", &OIIO::geterror);
    m.def("attribute", [](const std::string& name, float val) {
        OIIO::attribute(name, val);
    });
    m.def("attribute",
          [](const std::string& name, int val) { OIIO::attribute(name, val); });
    m.def("attribute", [](const std::string& name, const std::string& val) {
        OIIO::attribute(name, val);
    });
    m.def("attribute",
          [](const std::string& name, TypeDesc type, const py::tuple& obj) {
              oiio_attribute_typed(name, type, obj);
          });

    m.def(
        "get_int_attribute",
        [](const std::string& name, int def) {
            return OIIO::get_int_attribute(name, def);
        },
        py::arg("name"), py::arg("defaultval") = 0);
    m.def(
        "get_float_attribute",
        [](const std::string& name, float def) {
            return OIIO::get_float_attribute(name, def);
        },
        py::arg("name"), py::arg("defaultval") = 0.0f);
    m.def(
        "get_string_attribute",
        [](const std::string& name, const std::string& def) {
            return PY_STR(std::string(OIIO::get_string_attribute(name, def)));
        },
        py::arg("name"), py::arg("defaultval") = "");
    m.def("getattribute", &oiio_getattribute_typed);
    m.attr("AutoStride")          = AutoStride;
    m.attr("openimageio_version") = OIIO_VERSION;
    m.attr("VERSION")             = OIIO_VERSION;
    m.attr("VERSION_STRING")      = PY_STR(OIIO_VERSION_STRING);
    m.attr("VERSION_MAJOR")       = OIIO_VERSION_MAJOR;
    m.attr("VERSION_MINOR")       = OIIO_VERSION_MINOR;
    m.attr("VERSION_PATCH")       = OIIO_VERSION_PATCH;
    m.attr("INTRO_STRING")        = PY_STR(OIIO_INTRO_STRING);
    m.attr("__version__")         = PY_STR(OIIO_VERSION_STRING);
}

}  // namespace PyOpenImageIO
