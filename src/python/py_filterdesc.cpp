// Copyright Contributors to the OpenImageIO project.
// SPDX-License-Identifier: Apache-2.0
// https://github.com/AcademySoftwareFoundation/OpenImageIO

#include "py_oiio.h"
#include <OpenImageIO/filter.h>

namespace PyOpenImageIO {

void
declare_filterdesc(py_module& m)
{
    py::class_<FilterDesc>(m, "FilterDesc")
        .OIIO_PY_RO("name", &FilterDesc::name)
        .OIIO_PY_RO("dim", &FilterDesc::dim)
        .OIIO_PY_RO("width", &FilterDesc::width)
        .OIIO_PY_RO("fixedwidth", &FilterDesc::fixedwidth)
        .OIIO_PY_RO("scalable", &FilterDesc::scalable)
        .OIIO_PY_RO("separable", &FilterDesc::separable)
        .def("to_dict",
             [](const FilterDesc& f) {
#if defined(OIIO_PY_BACKEND_NANOBIND)
                 py::dict result;
                 result["name"]       = f.name;
                 result["dim"]        = f.dim;
                 result["width"]      = f.width;
                 result["fixedwidth"] = f.fixedwidth;
                 result["scalable"]   = f.scalable;
                 result["separable"]  = f.separable;
                 return result;
#else
                 return py::dict("name"_a = f.name, "dim"_a = f.dim,
                                 "width"_a      = f.width,
                                 "fixedwidth"_a = f.fixedwidth,
                                 "scalable"_a   = f.scalable,
                                 "separable"_a  = f.separable);
#endif
             })
        .def("__repr__",
             [](const FilterDesc& f) {
                 return Strutil::fmt::format("<FilterDesc '{}' {}D>", f.name,
                                             f.dim);
             })
        .def(
            "__eq__",
            [](const FilterDesc& self, const FilterDesc& other) {
                return self.name == other.name && self.dim == other.dim;
            },
            py::is_operator());

    py::class_<Filter1D>(m, "Filter1D")
        .def_static("num_filters", &Filter1D::num_filters)
        .def_static(
            "get_filterdesc",
            [](int filternum) -> std::optional<FilterDesc> {
                if (filternum >= 0 && filternum < Filter1D::num_filters()) {
                    return Filter1D::get_filterdesc(filternum);
                }
                return std::nullopt;
            },
            "filternum"_a);

    py::class_<Filter2D>(m, "Filter2D")
        .def_static("num_filters", &Filter2D::num_filters)
        .def_static(
            "get_filterdesc",
            [](int filternum) -> std::optional<FilterDesc> {
                if (filternum >= 0 && filternum < Filter2D::num_filters()) {
                    return Filter2D::get_filterdesc(filternum);
                }
                return std::nullopt;
            },
            "filternum"_a);
}

}  // namespace PyOpenImageIO
