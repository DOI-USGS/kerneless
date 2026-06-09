# utcet

UTC to Ephemeris Time conversion utilities - a Python binding for high-performance time conversion.

## Installation

```bash
pip install .
```

For development with tests:
```bash
pip install -e ".[test]"
```

## Usage

```python
import utcet

# Convert calendar time to ephemeris time
et = utcet.calendar_time_to_ephem_time("2000-01-01T12:00:00")
print(f"Ephemeris Time: {et}")

# Convert ephemeris time back to calendar time
cal = utcet.ephem_time_to_calendar_time(et, prec=3)
print(f"Calendar Time: {cal}")
```

## Testing

Run tests comparing against NAIF SPICE implementation:

```bash
pip install pytest spiceypy
pytest tests/
```

## Requirements

- Python >= 3.7
- pybind11 >= 2.6.0
- C++11 compatible compiler
