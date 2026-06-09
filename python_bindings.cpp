#include <pybind11/pybind11.h>
#include "utcet.h"

namespace py = pybind11;

PYBIND11_MODULE(utcet, m) {
    m.doc() = "UTC to Ephemeris Time conversion utilities";

    m.def("calendarTimeToEphemTime", &calendarTimeToEphemTime,
          py::arg("calendarTime"),
          "Convert calendar time string to ephemeris time.\n\n"
          "Args:\n"
          "    calendarTime: UTC time string in format 'YYYY-MM-DDTHH:MM:SS'\n\n"
          "Returns:\n"
          "    Ephemeris time in seconds since J2000 epoch");

    m.def("ephemTimeToCalendarTime", &ephemTimeToCalendarTime,
          py::arg("ephemTime"),
          py::arg("format") = "%Y-%m-%dT%H:%M:%S",
          py::arg("prec") = 3,
          py::arg("utclen") = 64,
          "Convert ephemeris time to calendar time string.\n\n"
          "Args:\n"
          "    ephemTime: Ephemeris time in seconds since J2000 epoch\n"
          "    format: strftime format string (default: '%Y-%m-%dT%H:%M:%S')\n"
          "    prec: Number of fractional second digits (default: 3)\n"
          "    utclen: Buffer size for output string (default: 64)");
}
