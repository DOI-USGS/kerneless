# Changelog

All changes that impact users of this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!---
This document is intended for users of the library and its Python bindings. Changes to
things like tests should not be noted in this document.

When updating this file for a PR, add an entry for your change under Unreleased
and one of the following headings:
 - Added - for new features.
 - Changed - for changes in existing functionality.
 - Deprecated - for soon-to-be removed features.
 - Removed - for now removed features.
 - Fixed - for any bug fixes.
 - Security - in case of vulnerabilities.

If the heading does not yet exist under Unreleased, then add it as a 3rd heading,
with three #.


When preparing for a public release candidate add a new 2nd heading, with two #, under
Unreleased with the version number and the release date, in year-month-day
format. Then, add a link for the new version at the bottom of this document and
update the Unreleased link so that it compares against the latest release tag.


When preparing for a bug fix release create a new 2nd heading above the Fixed
heading to indicate that only the bug fixes and security fixes are in the bug fix
release.
-->

## [Unreleased]

## [1.0.0] - 2026-10-16

First provisional release. This software has not received final approval by the
U.S. Geological Survey; see [DISCLAIMER.md](DISCLAIMER.md).

### Added
- `kerneless/utcet.h`, a header-only C++11 reimplementation of NAIF's `str2et_c` /
  `utc2et_c` / `et2utc_c` that needs no leap second kernel (LSK). The leap second table
  is embedded in the header. Accepts the calendar formats `str2et_c` does, including ISO
  8601, numeric dates in either field order, month names, day-of-year, partial dates, and
  a trailing `UTC`/`TDB`/`TDT`/`TT` designator; emits the `et2utc_c` format codes `C`,
  `D`, `ISOC`, `ISOD`, and `J`. Validated against SPICE to microsecond precision across
  1000+ random timestamps.
- `kerneless/dsk_reader.h`, a header-only, dependency-free C reader for NAIF DSK type-2
  (triangular plate model) kernels in the DAS binary format, mirroring `dasopr_c`,
  `dascls_c`, `dlabfs_c`, `dlafns_c`, `dskgd_c`, `dskz02_c`, `dskv02_c`, and `dskp02_c`.
  Links no cspice. Vertices and plates are validated byte-for-byte against cspice reading
  the same file. Originated in PSMRTS; see "Third-party content" in
  [LICENSE.md](LICENSE.md).
- Python bindings for both headers in a single `kerneless` extension module. Time
  conversion keeps the C++ function names and parameters; DSK reading is exposed as
  `kerneless.DskFile`, an RAII context manager whose methods keep the cspice call names
  but return values instead of filling caller buffers, with `room=0` meaning "all
  remaining elements".
- `pip install` places both headers at `<prefix>/include/kerneless/`, so C/C++ consumers
  of an installed copy can `#include <kerneless/utcet.h>` directly.

### Changed
- Renamed the project from `utcet` to `kerneless`, and the Python module with it. The
  header formerly at the repository root is now `include/kerneless/utcet.h`.
- Reported the license as CC0-1.0 throughout, matching `LICENSE.md`.

### Fixed
- Added an include guard to `utcet.h` and marked its two public functions `inline`.
  Without these, including the header from more than one translation unit failed to link.

[Unreleased]: https://github.com/DOI-USGS/kerneless/compare/1.0.0...HEAD
[1.0.0]: https://github.com/DOI-USGS/kerneless/releases/tag/1.0.0
