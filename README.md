# utcet

Header-only C++ library reimplementing NAIF's `utc2et` and `et2utc` without requiring leap second kernels (LSK files). Includes Python bindings via pybind11.

## Features

- **No external dependencies**: Leap second table embedded directly in code
- **Header-only**: Single `utcet.h` file for C++ projects
- **Python bindings**: Fast native extension module
- **NAIF-compatible**: Validated against SPICE toolkit (microsecond precision)
- **Lightweight**: No kernel file loading or management required

## Installation

```bash
pip install .
```

For development:
```bash
pip install -e ".[test]"
```

## Usage

### Python

Python bindings use identical function names and parameters as C++:

```python
import utcet

# UTC to Ephemeris Time
et = utcet.calendarTimeToEphemTime("2000-01-01T12:00:00")

# Ephemeris Time to UTC (all parameters match C++ API)
utc = utcet.ephemTimeToCalendarTime(et, format="%Y-%m-%dT%H:%M:%S", prec=3, utclen=64)

# Use defaults
utc = utcet.ephemTimeToCalendarTime(et)  # ISO 8601 with 3 decimal places
```

### C++

```cpp
#include "utcet.h"

// UTC to Ephemeris Time
double et = calendarTimeToEphemTime("2000-01-01T12:00:00");

// Ephemeris Time to UTC
std::string utc = ephemTimeToCalendarTime(et, "%Y-%m-%dT%H:%M:%S", 3, 64);
```

## Testing

Tests compare against NAIF SPICE across 1000+ random timestamps:

```bash
pip install pytest spiceypy
pytest tests/ -v
```

## Requirements

- Python >= 3.7
- pybind11 >= 2.6.0
- C++11 compatible compiler
