import pytest
import kerneless

try:
    import spiceypy as spice
    HAS_SPICEYPY = True
except ImportError:
    HAS_SPICEYPY = False


@pytest.mark.skipif(not HAS_SPICEYPY, reason="spiceypy not installed")
class TestUtcetVsNaif:
    """Compare kerneless' utcet.h implementation against NAIF SPICE."""

    @classmethod
    def setup_class(cls):
        """Load NAIF leap second kernel."""
        # Download naif0012.tls if needed
        import urllib.request
        import os

        # Get the directory containing this test file
        test_dir = os.path.dirname(os.path.abspath(__file__))
        kernel_file = os.path.join(test_dir, "naif0012.tls")

        if not os.path.exists(kernel_file):
            url = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls"
            print(f"Downloading NAIF kernel to {kernel_file}...")
            urllib.request.urlretrieve(url, kernel_file)

        spice.furnsh(kernel_file)

    @classmethod
    def teardown_class(cls):
        """Unload SPICE kernels."""
        if HAS_SPICEYPY:
            spice.kclear()

    def test_j2000_epoch(self):
        """Test conversion at J2000 epoch (2000-01-01T12:00:00)."""
        utc_time = "2000-01-01T12:00:00"

        # Our implementation
        et_utcet = kerneless.calendarTimeToEphemTime(utc_time)

        # NAIF SPICE
        et_spice = spice.str2et(utc_time)

        # Should be very close (within microsecond precision)
        assert et_utcet == pytest.approx(et_spice, abs=1e-6), \
            f"utcet: {et_utcet}, SPINAIFCE: {et_spice}, diff: {abs(et_utcet - et_spice)}"

    def test_multiple_dates(self):
        """Test conversion across multiple dates."""
        test_dates = [
            "1980-01-01T00:00:00",
            "1990-06-15T12:30:45",
            "2000-01-01T00:00:00",
            "2010-12-31T23:59:59",
            "2020-03-15T18:22:33",
        ]

        for utc_time in test_dates:
            et_utcet = kerneless.calendarTimeToEphemTime(utc_time)
            et_spice = spice.str2et(utc_time)

            assert et_utcet == pytest.approx(et_spice, abs=1e-6), \
                f"Date: {utc_time}, utcet: {et_utcet}, NAIF: {et_spice}, diff: {abs(et_utcet - et_spice)}"

    def test_leap_second_boundaries(self):
        """Test dates around leap second insertions."""
        leap_second_dates = [
            "1972-06-30T23:59:59",  # Before first leap second
            "1972-07-01T00:00:00",  # After first leap second
            "2015-06-30T23:59:59",  # Before 2015 leap second
            "2015-07-01T00:00:00",  # After 2015 leap second
            "2017-01-01T00:00:00",  # After most recent leap second
        ]

        for utc_time in leap_second_dates:
            et_utcet = kerneless.calendarTimeToEphemTime(utc_time)
            et_spice = spice.str2et(utc_time)
            print(f"Testing leap second boundary: {utc_time}, utcet: {et_utcet}, NAIF: {et_spice}")
            assert et_utcet == pytest.approx(et_spice, abs=1e-6), \
                f"Date: {utc_time}, utcet: {et_utcet}, NAIF: {et_spice}, diff: {abs(et_utcet - et_spice)}"

    def test_round_trip(self):
        """Test converting to ET and back to calendar time."""
        original_time = "2010-05-20T14:30:45"

        # Convert to ET
        et = kerneless.calendarTimeToEphemTime(original_time)

        # Convert back to calendar time
        calendar_time = kerneless.ephemTimeToCalendarTime(et, prec=0)

        # Should match (without fractional seconds since prec=0)
        assert calendar_time.startswith(original_time), \
            f"Original: {original_time}, Round-trip: {calendar_time}"

    def test_round_trip_with_microseconds(self):
        """Test round trip with microsecond precision for 2016-11-26 22:32:14.582000."""
        original_time = "2016-11-26T22:32:14.582000"

        # Convert to ET
        et_utcet = kerneless.calendarTimeToEphemTime(original_time)
        et_spice = spice.str2et(original_time)

        print(f"Original UTC: {original_time}, ET (utcet): {et_utcet}, ET (SPICE): {et_spice}")
        print(f"Difference in ET: {abs(et_utcet - et_spice)} seconds")

        # Verify ET matches SPICE
        assert et_utcet == pytest.approx(et_spice, abs=1e-6), \
            f"ET mismatch: utcet={et_utcet}, SPICE={et_spice}"

        # Convert back with millisecond precision
        calendar_utcet = kerneless.ephemTimeToCalendarTime(et_utcet, prec=3)
        calendar_spice = spice.et2utc(et_spice, 'ISOC', 3)

        # Both should match exactly now (no 'Z' suffix)
        assert calendar_utcet == calendar_spice, \
            f"Round-trip failed: utcet={calendar_utcet}, SPICE={calendar_spice}"

    def test_inverse_comparison(self):
        """Test inverse conversion (ET to calendar) against SPICE."""
        test_et_values = [0.0, 1e8, -1e8, 5e8]

        for et in test_et_values:
            # Our implementation
            cal_utcet = kerneless.ephemTimeToCalendarTime(et, prec=3)

            # NAIF SPICE
            cal_spice = spice.et2utc(et, 'ISOC', 3)

            # Both formats: YYYY-MM-DDTHH:MM:SS.sss
            # Parse and compare with tolerance for rounding differences
            from datetime import datetime
            utcet_time = datetime.fromisoformat(cal_utcet)
            spice_time = datetime.fromisoformat(cal_spice)

            utcet_ts = utcet_time.timestamp()
            spice_ts = spice_time.timestamp()

            # Allow up to 2ms difference for rounding
            assert utcet_ts == pytest.approx(spice_ts, abs=0.002, rel=0), \
                f"ET: {et}, utcet: {cal_utcet}, NAIF: {cal_spice}"

    def test_random_utc_to_et_conversions(self):
        """Test 1000 random UTC times converted to ET against SPICE."""
        import random
        from datetime import datetime, timedelta

        # Generate 1000 random UTC times between 1972 and 2025
        start_date = datetime(1972, 1, 1)
        end_date = datetime(2025, 12, 31)
        date_range_seconds = int((end_date - start_date).total_seconds())

        failures = []
        for i in range(1000):
            # Random offset in seconds
            random_seconds = random.randint(0, date_range_seconds)
            random_date = start_date + timedelta(seconds=random_seconds)

            # Format as ISO string (without microseconds)
            utc_time = random_date.strftime("%Y-%m-%dT%H:%M:%S")

            # Convert using both implementations
            et_utcet = kerneless.calendarTimeToEphemTime(utc_time)
            et_spice = spice.str2et(utc_time)

            # Check if within tolerance
            diff = abs(et_utcet - et_spice)
            if diff >= 1e-6:
                failures.append({
                    'utc': utc_time,
                    'utcet': et_utcet,
                    'spice': et_spice,
                    'diff': diff
                })

        # Report failures if any
        assert len(failures) == 0, \
            f"{len(failures)}/1000 conversions failed. First 5 failures: {failures[:5]}"


def test_basic_functionality():
    """Basic tests that don't require SPICE."""
    # Test that functions exist and run without error
    et = kerneless.calendarTimeToEphemTime("2000-01-01T12:00:00")
    assert isinstance(et, float)

    cal = kerneless.ephemTimeToCalendarTime(et)
    assert isinstance(cal, str)
    assert "2000-01-01T12:00:00" in cal
    assert not cal.endswith('Z'), "Output should not have 'Z' suffix"
