#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <kerneless/dsk_reader.h>
#include <kerneless/utcet.h>

namespace py = pybind11;

namespace {

// Thin RAII wrapper over the C handle in dsk_reader.h. The C reader keeps the
// open file in a caller-allocated handle (see the header's note on the cspice
// deviation); owning it in a class lets Python's refcount close the file and
// makes the reader usable as a context manager.
//
// Read methods mirror the cspice call names (dskgd_c, dskz02_c, ...) like the
// header does, but return values instead of filling caller buffers: a `room`
// of 0 means "every remaining element", and vertices/plates come back as lists
// of 3-element lists rather than flat arrays.
class DskFile {
 public:
  explicit DskFile(const std::string &path) : path_(path) {
    if (kdsk_dasopr(path.c_str(), &handle_) != 0) {
      throw std::runtime_error("Unable to open DSK file: \"" + path + "\"");
    }
  }

  ~DskFile() { kdsk_dascls(&handle_); }

  DskFile(const DskFile &) = delete;
  DskFile &operator=(const DskFile &) = delete;

  const std::string &path() const { return path_; }
  bool isOpen() const { return handle_.ok != 0; }
  void close() { kdsk_dascls(&handle_); }

  // dlabfs_c: descriptor of the first DLA segment, or None when the file holds
  // no segments.
  py::object dlabfs() {
    requireOpen();
    kdsk_dladescr dla;
    int found = 0;
    kdsk_dlabfs(&handle_, &dla, &found);
    if (!found) {
      return py::none();
    }
    return py::cast(dla);
  }

  // dlafns_c: descriptor of the segment following dladsc, or None at the end
  // of the segment list.
  py::object dlafns(const kdsk_dladescr &dladsc) {
    requireOpen();
    kdsk_dladescr nxt;
    int found = 0;
    kdsk_dlafns(&handle_, &dladsc, &nxt, &found);
    if (!found) {
      return py::none();
    }
    return py::cast(nxt);
  }

  // Every DLA segment, in file order (dlabfs_c then dlafns_c to exhaustion).
  std::vector<kdsk_dladescr> segments() {
    requireOpen();
    std::vector<kdsk_dladescr> out;
    kdsk_dladescr dla;
    int found = 0;
    kdsk_dlabfs(&handle_, &dla, &found);
    while (found) {
      out.push_back(dla);
      kdsk_dladescr nxt;
      kdsk_dlafns(&handle_, &dla, &nxt, &found);
      dla = nxt;
    }
    return out;
  }

  // dskgd_c: the segment's DSK descriptor.
  kdsk_dskdescr dskgd(const kdsk_dladescr &dladsc) {
    requireOpen();
    kdsk_dskdescr dskdsc;
    if (kdsk_dskgd(&handle_, &dladsc, &dskdsc) != 0) {
      throw std::runtime_error("dskgd: unable to read the DSK descriptor");
    }
    return dskdsc;
  }

  // dskz02_c: (vertex count, plate count) of a type-2 segment.
  std::pair<int32_t, int32_t> dskz02(const kdsk_dladescr &dladsc) {
    requireOpen();
    int32_t nv = 0, np = 0;
    if (kdsk_dskz02(&handle_, &dladsc, &nv, &np) != 0) {
      throw std::runtime_error("dskz02: unable to read the segment size");
    }
    return {nv, np};
  }

  // dskv02_c: vertices [start, start + room) as [x, y, z] lists, in km.
  std::vector<std::array<double, 3>> dskv02(const kdsk_dladescr &dladsc,
                                            int32_t start, int32_t room) {
    int32_t count = clampRoom(dskz02(dladsc).first, start, room);
    std::vector<std::array<double, 3>> vrtces((size_t)count);
    if (count == 0) {
      return vrtces;
    }
    int32_t n = 0;
    if (kdsk_dskv02(&handle_, &dladsc, start, count, &n, &vrtces[0][0]) != 0) {
      throw std::runtime_error("dskv02: unable to read vertices");
    }
    vrtces.resize((size_t)n);
    return vrtces;
  }

  // dskp02_c: plates [start, start + room) as [i, j, k] lists of 1-based
  // vertex indices. Callers building 0-based meshes must subtract 1.
  std::vector<std::array<int32_t, 3>> dskp02(const kdsk_dladescr &dladsc,
                                             int32_t start, int32_t room) {
    int32_t count = clampRoom(dskz02(dladsc).second, start, room);
    std::vector<std::array<int32_t, 3>> plates((size_t)count);
    if (count == 0) {
      return plates;
    }
    int32_t n = 0;
    if (kdsk_dskp02(&handle_, &dladsc, start, count, &n, &plates[0][0]) != 0) {
      throw std::runtime_error("dskp02: unable to read plates");
    }
    plates.resize((size_t)n);
    return plates;
  }

 private:
  void requireOpen() {
    if (!isOpen()) {
      throw std::runtime_error("DSK file is closed: \"" + path_ + "\"");
    }
  }

  // Number of elements actually readable from `start` given `total` in the
  // segment; room <= 0 requests all of them.
  static int32_t clampRoom(int32_t total, int32_t start, int32_t room) {
    if (start < 1) {
      throw std::invalid_argument("start must be >= 1 (DSK indices are 1-based)");
    }
    if (start > total) {
      return 0;
    }
    int32_t avail = total - start + 1;
    return (room > 0 && room < avail) ? room : avail;
  }

  std::string path_;
  kdsk_handle handle_;
};

}  // namespace

PYBIND11_MODULE(kerneless, m) {
  m.doc() =
      "kerneless: SPICE-compatible readers and time conversions that need no "
      "SPICE kernels or toolkit.\n\n"
      "utcet: UTC/ephemeris time conversion with no leap second kernel.\n"
      "dsk:   NAIF DSK type-2 plate model reading with no cspice.";

  // ── utcet.h ────────────────────────────────────────────────────────────
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

  // ── dsk_reader.h ───────────────────────────────────────────────────────
  // Coordinate system codes reported by DskDescriptor.corsys.
  m.attr("LATSYS") = KDSK_LATSYS;
  m.attr("CYLSYS") = KDSK_CYLSYS;
  m.attr("RECSYS") = KDSK_RECSYS;
  m.attr("PDTSYS") = KDSK_PDTSYS;

  py::class_<kdsk_dladescr>(m, "DlaDescriptor",
                            "DLA segment descriptor (mirrors SpiceDLADescr). "
                            "Base addresses are the predecessors of the first "
                            "element: data occupy base+1 .. base+size.")
      .def_readonly("bwdptr", &kdsk_dladescr::bwdptr,
                    "Backward pointer (DAS integer address; -1 = none).")
      .def_readonly("fwdptr", &kdsk_dladescr::fwdptr,
                    "Forward pointer (DAS integer address; -1 = none).")
      .def_readonly("ibase", &kdsk_dladescr::ibase, "Base DAS integer address.")
      .def_readonly("isize", &kdsk_dladescr::isize,
                    "Integer component element count.")
      .def_readonly("dbase", &kdsk_dladescr::dbase, "Base DAS double address.")
      .def_readonly("dsize", &kdsk_dladescr::dsize,
                    "Double component element count.")
      .def_readonly("cbase", &kdsk_dladescr::cbase,
                    "Base DAS character address.")
      .def_readonly("csize", &kdsk_dladescr::csize,
                    "Character component element count.")
      .def("__repr__", [](const kdsk_dladescr &d) {
        return "<DlaDescriptor ibase=" + std::to_string(d.ibase) +
               " isize=" + std::to_string(d.isize) +
               " dbase=" + std::to_string(d.dbase) +
               " dsize=" + std::to_string(d.dsize) + ">";
      });

  py::class_<kdsk_dskdescr>(m, "DskDescriptor",
                            "DSK segment descriptor (mirrors SpiceDSKDescr), "
                            "as returned by DskFile.dskgd().")
      .def_readonly("surfce", &kdsk_dskdescr::surfce, "Surface ID code.")
      .def_readonly("center", &kdsk_dskdescr::center, "Center (body) ID code.")
      .def_readonly("dclass", &kdsk_dskdescr::dclass, "Data class code.")
      .def_readonly("dtype", &kdsk_dskdescr::dtype,
                    "DSK data type (2 for plate models).")
      .def_readonly("frmcde", &kdsk_dskdescr::frmcde,
                    "Reference frame ID code.")
      .def_readonly("corsys", &kdsk_dskdescr::corsys,
                    "Coordinate system code (LATSYS/CYLSYS/RECSYS/PDTSYS).")
      .def_property_readonly(
          "corpar",
          [](const kdsk_dskdescr &d) {
            return std::vector<double>(d.corpar, d.corpar + KDSK_NSYPAR);
          },
          "Coordinate system parameters (10 doubles).")
      .def_readonly("co1min", &kdsk_dskdescr::co1min, "First coordinate min.")
      .def_readonly("co1max", &kdsk_dskdescr::co1max, "First coordinate max.")
      .def_readonly("co2min", &kdsk_dskdescr::co2min, "Second coordinate min.")
      .def_readonly("co2max", &kdsk_dskdescr::co2max, "Second coordinate max.")
      .def_readonly("co3min", &kdsk_dskdescr::co3min, "Third coordinate min.")
      .def_readonly("co3max", &kdsk_dskdescr::co3max, "Third coordinate max.")
      .def_readonly("start", &kdsk_dskdescr::start,
                    "Coverage start (seconds past J2000).")
      .def_readonly("stop", &kdsk_dskdescr::stop,
                    "Coverage stop (seconds past J2000).")
      .def("__repr__", [](const kdsk_dskdescr &d) {
        return "<DskDescriptor dtype=" + std::to_string(d.dtype) +
               " center=" + std::to_string(d.center) +
               " surfce=" + std::to_string(d.surfce) +
               " frmcde=" + std::to_string(d.frmcde) + ">";
      });

  py::class_<DskFile>(m, "DskFile",
                      "A NAIF DSK (.bds) file opened for reading. Read methods "
                      "mirror the cspice DSK call names; a room of 0 reads "
                      "every remaining element.")
      .def(py::init<const std::string &>(), py::arg("path"),
           "Open a DSK file for read access (dasopr_c).\n\n"
           "Raises:\n"
           "    RuntimeError: if the file cannot be opened or is not a DAS "
           "file.")
      .def_property_readonly("path", &DskFile::path, "Path the file was opened from.")
      .def_property_readonly("is_open", &DskFile::isOpen,
                             "True until close() is called.")
      .def("close", &DskFile::close, "Close the file (dascls_c). Idempotent.")
      .def("dlabfs", &DskFile::dlabfs,
           "Descriptor of the first DLA segment, or None if the file has no "
           "segments (dlabfs_c).")
      .def("dlafns", &DskFile::dlafns, py::arg("dladsc"),
           "Descriptor of the segment following dladsc, or None at the end of "
           "the list (dlafns_c).")
      .def("segments", &DskFile::segments,
           "List of every DLA segment descriptor, in file order.")
      .def("dskgd", &DskFile::dskgd, py::arg("dladsc"),
           "DSK descriptor of a segment (dskgd_c).")
      .def("dskz02", &DskFile::dskz02, py::arg("dladsc"),
           "(vertex count, plate count) of a type-2 segment (dskz02_c).")
      .def("dskv02", &DskFile::dskv02, py::arg("dladsc"), py::arg("start") = 1,
           py::arg("room") = 0,
           "Vertices of a type-2 segment as [x, y, z] lists, in km "
           "(dskv02_c).\n\n"
           "Args:\n"
           "    dladsc: Segment descriptor from dlabfs()/segments()\n"
           "    start: 1-based index of the first vertex (default: 1)\n"
           "    room: Maximum vertices to read; 0 reads all remaining")
      .def("dskp02", &DskFile::dskp02, py::arg("dladsc"), py::arg("start") = 1,
           py::arg("room") = 0,
           "Plates of a type-2 segment as [i, j, k] lists of 1-based vertex indices "
           "(dskp02_c).\n\n"
           "Args:\n"
           "    dladsc: Segment descriptor from dlabfs()/segments()\n"
           "    start: 1-based index of the first plate (default: 1)\n"
           "    room: Maximum plates to read; 0 reads all remaining")
      .def("__enter__", [](DskFile &f) { return &f; },
           py::return_value_policy::reference_internal)
      .def("__exit__",
           [](DskFile &f, const py::object &, const py::object &,
              const py::object &) {
             f.close();
             return false;
           })
      .def("__repr__", [](const DskFile &f) {
        return "<DskFile '" + f.path() + "'" +
               (f.isOpen() ? ">" : " (closed)>");
      });
}
