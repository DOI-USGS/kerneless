"""Tests for the header-only DSK reader (dsk_reader.h) exposed as kerneless.DskFile.

The golden fixture is bennu_20facets.bds, a NAIF DSK type-2 (triangular plate)
segment with NV=20 and NP=36. If the DAS address arithmetic ever regresses,
these fire first.
"""

import math
import os

import pytest
import kerneless

try:
    import spiceypy as spice
    HAS_SPICEYPY = True
except ImportError:
    HAS_SPICEYPY = False

BDS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "data", "bennu_20facets.bds")

# Golden counts for bennu_20facets.bds.
NV = 20
NP = 36


@pytest.fixture
def dsk():
    with kerneless.DskFile(BDS_FILE) as f:
        yield f


@pytest.fixture
def segment(dsk):
    return dsk.dlabfs()


def test_open_missing_file_raises():
    with pytest.raises(RuntimeError):
        kerneless.DskFile(os.path.join(os.path.dirname(BDS_FILE), "no_such.bds"))


def test_open_non_das_file_raises(tmp_path):
    """A file that does not start with a DAS idword must be rejected."""
    bogus = tmp_path / "bogus.bds"
    bogus.write_bytes(b"\x00" * 2048)
    with pytest.raises(RuntimeError):
        kerneless.DskFile(str(bogus))


def test_close_is_idempotent_and_blocks_reads():
    f = kerneless.DskFile(BDS_FILE)
    assert f.is_open
    seg = f.dlabfs()
    f.close()
    f.close()
    assert not f.is_open
    with pytest.raises(RuntimeError):
        f.dskz02(seg)


def test_single_segment(dsk):
    segments = dsk.segments()
    assert len(segments) == 1
    assert dsk.dlafns(segments[0]) is None


def test_dskz02_counts(dsk, segment):
    assert dsk.dskz02(segment) == (NV, NP)


def test_dskgd_is_type_2_plate_model(dsk, segment):
    d = dsk.dskgd(segment)
    assert d.dtype == 2
    assert d.corsys in (kerneless.LATSYS, kerneless.CYLSYS,
                        kerneless.RECSYS, kerneless.PDTSYS)
    assert len(d.corpar) == 10
    assert d.co1min <= d.co1max
    assert d.start <= d.stop


def test_vertices_are_plausible_bennu_radii(dsk, segment):
    """Bennu's mean radius is ~0.245 km; allow a generous 2 km bound."""
    vertices = dsk.dskv02(segment)
    assert len(vertices) == NV
    for x, y, z in vertices:
        assert math.isfinite(x) and math.isfinite(y) and math.isfinite(z)
        r = math.sqrt(x * x + y * y + z * z)
        assert 0.01 < r < 2.0


def test_plate_indices_are_1_based_and_in_range(dsk, segment):
    plates = dsk.dskp02(segment)
    assert len(plates) == NP
    for a, b, c in plates:
        assert 1 <= a <= NV
        assert 1 <= b <= NV
        assert 1 <= c <= NV
        assert len({a, b, c}) == 3, "a plate must not reuse a vertex"


def test_every_vertex_is_referenced(dsk, segment):
    """A closed 20-vertex/36-plate hull must use all of its vertices."""
    used = {i for plate in dsk.dskp02(segment) for i in plate}
    assert used == set(range(1, NV + 1))


def test_start_and_room_windowing(dsk, segment):
    all_vertices = dsk.dskv02(segment)

    # room caps the read; start is 1-based.
    assert dsk.dskv02(segment, room=5) == all_vertices[:5]
    assert dsk.dskv02(segment, start=3, room=4) == all_vertices[2:6]

    # room of 0 (the default) reads everything remaining.
    assert dsk.dskv02(segment, start=18) == all_vertices[17:]

    # room past the end of the segment is clamped, not an error.
    assert dsk.dskv02(segment, start=19, room=100) == all_vertices[18:]

    # start past the end yields nothing.
    assert dsk.dskv02(segment, start=NV + 1) == []
    assert dsk.dskp02(segment, start=NP + 1) == []


def test_start_below_one_raises(dsk, segment):
    with pytest.raises(ValueError):
        dsk.dskv02(segment, start=0)
    with pytest.raises(ValueError):
        dsk.dskp02(segment, start=-1)


def test_dla_descriptor_fields(dsk, segment):
    assert segment.bwdptr == -1, "the only segment has no predecessor"
    assert segment.fwdptr == -1, "the only segment has no successor"
    assert segment.isize > 0 and segment.dsize > 0
    assert "DlaDescriptor" in repr(segment)


@pytest.mark.skipif(not HAS_SPICEYPY, reason="spiceypy not installed")
class TestDskVsNaif:
    """Compare the kernel-free reader against cspice reading the same file."""

    @classmethod
    def setup_class(cls):
        cls.handle = spice.dasopr(BDS_FILE)
        cls.dladsc = spice.dlabfs(cls.handle)

    @classmethod
    def teardown_class(cls):
        spice.dascls(cls.handle)

    def test_counts_match(self, dsk, segment):
        assert dsk.dskz02(segment) == tuple(spice.dskz02(self.handle, self.dladsc))

    def test_descriptor_matches(self, dsk, segment):
        ours = dsk.dskgd(segment)
        theirs = spice.dskgd(self.handle, self.dladsc)
        assert ours.surfce == theirs.surfce
        assert ours.center == theirs.center
        assert ours.dclass == theirs.dclass
        assert ours.dtype == theirs.dtype
        assert ours.frmcde == theirs.frmcde
        assert ours.corsys == theirs.corsys
        assert ours.co1min == pytest.approx(theirs.co1min)
        assert ours.co1max == pytest.approx(theirs.co1max)
        assert ours.co2min == pytest.approx(theirs.co2min)
        assert ours.co2max == pytest.approx(theirs.co2max)
        assert ours.co3min == pytest.approx(theirs.co3min)
        assert ours.co3max == pytest.approx(theirs.co3max)
        assert ours.start == pytest.approx(theirs.start)
        assert ours.stop == pytest.approx(theirs.stop)

    def test_vertices_match_bit_for_bit(self, dsk, segment):
        ours = dsk.dskv02(segment)
        theirs = spice.dskv02(self.handle, self.dladsc, 1, NV)
        assert len(ours) == len(theirs)
        for mine, naif in zip(ours, theirs):
            # Both read the same doubles off disk, so require exact equality.
            assert list(mine) == list(naif)

    def test_plates_match(self, dsk, segment):
        ours = dsk.dskp02(segment)
        theirs = spice.dskp02(self.handle, self.dladsc, 1, NP)
        assert len(ours) == len(theirs)
        for mine, naif in zip(ours, theirs):
            assert list(mine) == list(naif)
