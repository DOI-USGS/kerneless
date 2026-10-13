# kerneless

Header-only reimplementations of NAIF SPICE functionality that need no SPICE
kernels and no SPICE toolkit. Includes Python bindings via pybind11.

| Header | Language | Replaces | Needs |
| --- | --- | --- | --- |
| [kerneless/utcet.h](include/kerneless/utcet.h) | C++11 | `str2et_c` / `utc2et_c` / `et2utc_c` | no leap second kernel (LSK) |
| [kerneless/dsk_reader.h](include/kerneless/dsk_reader.h) | C (usable from C++) | `dasopr_c` / `dlabfs_c` / `dskgd_c` / `dskz02_c` / `dskv02_c` / `dskp02_c` | no cspice link |

## Features

- **No external dependencies**: the leap second table is embedded in code, and
  `.bds` DSK files are parsed straight off disk
- **Header-only**: drop a single header into your project
- **Python bindings**: one native extension module, `kerneless`
- **NAIF-compatible**: validated against the SPICE toolkit (microsecond
  precision for time; byte-for-byte for DSK vertices and plates)

## Installation

```bash
pip install .
```

For development:

```bash
pip install -e ".[test]"
```

`pip install` also lays the headers down under the install prefix, so C/C++
consumers of an installed copy can compile against them directly:

```bash
cc -I "$(python -c 'import sysconfig; print(sysconfig.get_path("data"))')/include" ...
```

Or skip the install entirely: add this repository's [include/](include/) to your
include path, or copy the single header you need. There is nothing to build or
link.

### If `import kerneless` looks empty

```python
>>> dir(kerneless)
['__doc__', '__file__', '__loader__', '__name__', '__package__', '__path__', '__spec__']
```

That `__path__`, with no functions, means Python imported a *directory* named
`kerneless` as an empty namespace package instead of the extension module. Two
directories in a clone can do it: the clone itself, and `include/kerneless/`.
Python puts the current directory at the front of `sys.path`, so running
`python` from the directory *containing* your clone shadows the install:

```bash
cd ~/repos && python -c "import kerneless"   # finds ~/repos/kerneless/, the clone
```

Run it from anywhere else, or keep the current directory off the import path:

```bash
python -P -c "import kerneless"              # or export PYTHONSAFEPATH=1
```

The same applies if `include/` ever lands on `PYTHONPATH` — it holds only
headers, so `include/kerneless/` imports as an empty namespace package too. It
belongs on your compiler's include path, not Python's.

This only affects editable installs (`pip install -e .`). A regular
`pip install` is immune: the extension module lands in `site-packages`, and a
real module always beats a namespace directory.

## Usage

### Time conversion (`kerneless/utcet.h`)

Python bindings use identical function names and parameters as C++:

```python
import kerneless

# UTC to Ephemeris Time
et = kerneless.calendarTimeToEphemTime("2000-01-01T12:00:00")

# Ephemeris Time to UTC (all parameters match C++ API)
utc = kerneless.ephemTimeToCalendarTime(et, format="%Y-%m-%dT%H:%M:%S", prec=3, utclen=64)

# Use defaults
utc = kerneless.ephemTimeToCalendarTime(et)  # ISO 8601 with 3 decimal places
```

```cpp
#include <kerneless/utcet.h>

// UTC to Ephemeris Time
double et = calendarTimeToEphemTime("2000-01-01T12:00:00");

// Ephemeris Time to UTC
std::string utc = ephemTimeToCalendarTime(et, "%Y-%m-%dT%H:%M:%S", 3, 64);
```

`calendarTimeToEphemTime` accepts the calendar formats `str2et_c` does
(ISO 8601, `YYYY/MM/DD`, `MM/DD/YYYY`, month names in any field order,
day-of-year, partial dates) plus a trailing `UTC`/`TDB`/`TDT`/`TT` designator.
`ephemTimeToCalendarTime` understands the `et2utc_c` format codes `C`, `D`,
`ISOC`, `ISOD`, and `J`.

### DSK plate models (`kerneless/dsk_reader.h`)

The Python API keeps the cspice call names, but returns values instead of
filling caller buffers: `room=0` means "all remaining elements", and vertices
and plates come back as lists of 3-element lists.

```python
import kerneless

with kerneless.DskFile("bennu.bds") as dsk:
    for segment in dsk.segments():        # dlabfs_c / dlafns_c
        descr = dsk.dskgd(segment)        # dskgd_c
        if descr.dtype != 2:              # only type-2 plate models are supported
            continue

        nv, np_ = dsk.dskz02(segment)     # dskz02_c
        vertices = dsk.dskv02(segment)    # dskv02_c -> [[x, y, z], ...] in km
        plates = dsk.dskp02(segment)      # dskp02_c -> [[i, j, k], ...], 1-based

        # A window of the segment, 1-based like SPICE
        first_ten = dsk.dskv02(segment, start=1, room=10)

        # Plate indices are 1-based, matching dskp02_c
        triangle = [vertices[i - 1] for i in plates[0]]
```

```c
#include <kerneless/dsk_reader.h>

kdsk_handle h;
if (kdsk_dasopr("bennu.bds", &h) == 0) {
    kdsk_dladescr seg;
    int found = 0;
    kdsk_dlabfs(&h, &seg, &found);          /* dlabfs_c */
    if (found) {
        int32_t nv = 0, np = 0;
        kdsk_dskz02(&h, &seg, &nv, &np);    /* dskz02_c */

        double vertices[3 * 20];
        int32_t nread = 0;
        kdsk_dskv02(&h, &seg, 1, 20, &nread, vertices);  /* dskv02_c */
    }
    kdsk_dascls(&h);                        /* dascls_c */
}
```

DAS addresses and plate vertex indices are 1-based, exactly as in SPICELIB, so
call sites can be swapped for the real `*_c` wrappers with little change. Two
deliberate differences from cspice:

- An open file is a caller-allocated `kdsk_handle` rather than an integer handle
  in a global table, which keeps the reader header-only and free of global state.
- The intersection routines (`dskx02_c` / `dskn02_c`) are not ported; the reader
  hands back plates and vertices for your own tracer to intersect.

## Testing

```bash
pip install pytest spiceypy
pytest tests/ -v
```

`tests/test_utcet_vs_naif.py` compares against NAIF SPICE across 1000+ random
timestamps. `tests/test_dsk.py` reads the golden `tests/data/bennu_20facets.bds`
type-2 segment (NV=20, NP=36) and, when spiceypy is installed, checks the
vertices, plates, and descriptor against cspice reading the same file. Tests
that need SPICE skip themselves when spiceypy is missing.

## Requirements

- Python >= 3.7
- pybind11 >= 2.6.0
- C++11 compatible compiler (`kerneless/utcet.h`, bindings); any C compiler
  for `kerneless/dsk_reader.h`

## Suggested Citation

Rodriguez, K., 2026, kerneless: Header-only SPICE-compatible time conversion and DSK
reading without SPICE kernels, version 0.1.0: U.S. Geological Survey software release,
https://github.com/DOI-USGS/kerneless.

A Digital Object Identifier (DOI) will be assigned when this software completes USGS
review and approval; cite the DOI in preference to the repository URL once it exists.

## Point of Contact

Kelvin Rodriguez, U.S. Geological Survey Astrogeology Science Center —
<krodriguez@usgs.gov>

## Contributing

Bug reports, feature requests, and pull requests are welcome from anyone, inside or
outside the USGS. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to report a problem or
submit a change, and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for the expected conduct.
Because this repository lives in the DOI GitHub Enterprise Cloud, merge rights are
limited to DOI staff — an external contribution lands by way of a maintainer merging the
pull request.

Do not open a public issue for a suspected security vulnerability; email the point of
contact above instead.

## Provenance

`include/kerneless/dsk_reader.h` originated in PSMRTS and is marked CC0 1.0 by the
OSIRIS-APEX project at the University of Arizona; `tests/data/bennu_20facets.bds` is a
NAIF DSK shape model vendored from the same project as a test fixture. Both are noted
under "Third-party content" in [LICENSE.md](LICENSE.md).

## Generative AI Disclosure

This repository contains software code that was generated or modified with the assistance
of artificial intelligence (AI) tools, in accordance with U.S. Geological Survey (USGS)
disclosure requirements. All AI-generated or AI-assisted code has been reviewed and
validated by USGS developers. Contributors using AI
tools are responsible for reviewing and testing their code.

## License

This project is in the public domain in the United States. See [LICENSE.md](LICENSE.md) for full details.

## Disclaimer

This software is preliminary or provisional and is subject to revision. See [DISCLAIMER.md](DISCLAIMER.md) for the complete disclaimer statement.
