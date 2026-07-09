#include <doctest/doctest.h>
#include "parsers/libp.h"

using namespace std;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Write a big-endian int16 into a vector at position pos
static void WriteBE16(std::vector<uint8_t>& buf, size_t pos, int16_t val) {
    buf[pos]     = (val >> 8) & 0xFF;
    buf[pos + 1] = val & 0xFF;
}

// Build a synthetic "binary" buffer large enough to hold track data at
// a given offset, containing N 12-byte coord segments.
static std::vector<uint8_t> MakeTrackBinary(
    uint32_t offset,
    const std::vector<SrTrackCoordSegment>& segments)
{
    size_t needed = offset + segments.size() * 12 + 12; // +12 for safety
    std::vector<uint8_t> buf(needed, 0);

    for (size_t i = 0; i < segments.size(); i++) {
        size_t base = offset + i * 12;
        const auto& s = segments[i];
        // Coordinates stored PRE-scaling (multiply by 16 to reverse ScaleCoordinates)
        WriteBE16(buf, base + 0,  (int16_t)(s.xStart * SR_COORDINATE_SCALE_FACTOR));
        WriteBE16(buf, base + 2,  (int16_t)(s.xEnd   * SR_COORDINATE_SCALE_FACTOR));
        WriteBE16(buf, base + 4,  (int16_t)(s.yStart * SR_COORDINATE_SCALE_FACTOR));
        WriteBE16(buf, base + 6,  (int16_t)(s.yEnd   * SR_COORDINATE_SCALE_FACTOR));
        WriteBE16(buf, base + 8,  s.sprite1);
        WriteBE16(buf, base + 10, s.sprite2);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// SrTrackSegment methods
// ---------------------------------------------------------------------------

TEST_SUITE("SrTrackSegment") {

TEST_CASE("IsEndOfChain: bit 1 of flags") {
    SrTrackSegment seg = {};

    seg.flags = 0;
    CHECK(seg.IsEndOfChain() == false);

    seg.flags = 1; // bit 0 only
    CHECK(seg.IsEndOfChain() == false);

    seg.flags = 2; // bit 1 set
    CHECK(seg.IsEndOfChain() == true);

    seg.flags = 3; // bits 0 and 1
    CHECK(seg.IsEndOfChain() == true);

    seg.flags = 4; // bit 2 only
    CHECK(seg.IsEndOfChain() == false);
}

TEST_CASE("GetBoundingBox returns the four trap fields") {
    SrTrackSegment seg = {};
    seg.trapXMin = -10;
    seg.trapXMax =  20;
    seg.trapYMin = -5;
    seg.trapYMax =  15;

    int16_t minX, minY, maxX, maxY;
    seg.GetBoundingBox(minX, minY, maxX, maxY);

    CHECK(minX == -10);
    CHECK(maxX ==  20);
    CHECK(minY ==  -5);
    CHECK(maxY ==  15);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Static coordinate/sprite scaling helpers
// ---------------------------------------------------------------------------

TEST_SUITE("TrackReader static helpers") {

TEST_CASE("ScaleCoordinates divides all four fields by SR_COORDINATE_SCALE_FACTOR") {
    SrTrackCoordSegment seg;
    seg.xStart = 160; seg.xEnd = 320;
    seg.yStart = 80;  seg.yEnd = 160;
    seg.sprite1 = 0;  seg.sprite2 = 0;

    TrackReader::ScaleCoordinates(seg);

    CHECK(seg.xStart == 160 / SR_COORDINATE_SCALE_FACTOR);
    CHECK(seg.xEnd   == 320 / SR_COORDINATE_SCALE_FACTOR);
    CHECK(seg.yStart ==  80 / SR_COORDINATE_SCALE_FACTOR);
    CHECK(seg.yEnd   == 160 / SR_COORDINATE_SCALE_FACTOR);
}

TEST_CASE("ScaleCoordinates: zero stays zero") {
    SrTrackCoordSegment seg = {};
    TrackReader::ScaleCoordinates(seg);
    CHECK(seg.xStart == 0);
    CHECK(seg.xEnd   == 0);
}

TEST_CASE("ScaleSpritePosition divides both position fields by SR_COORDINATE_SCALE_FACTOR") {
    SrRoadSideSpriteRecord rec;
    rec.spriteId1      = 31;
    rec.trackPosition1 = 160;
    rec.spriteId2      = 32;
    rec.trackPosition2 = 320;

    TrackReader::ScaleSpritePosition(rec);

    CHECK(rec.trackPosition1 == 160 / SR_COORDINATE_SCALE_FACTOR);
    CHECK(rec.trackPosition2 == 320 / SR_COORDINATE_SCALE_FACTOR);
    CHECK(rec.spriteId1 == 31); // sprite IDs unchanged
    CHECK(rec.spriteId2 == 32);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// TrackReader::LoadTrackData (vector overload)
// ---------------------------------------------------------------------------

TEST_SUITE("TrackReader::LoadTrackData") {

TEST_CASE("empty vector returns false and leaves reader not loaded") {
    TrackReader tr;
    CHECK(tr.LoadTrackData(std::vector<uint8_t>{}) == false);
}

TEST_CASE("non-empty vector returns true") {
    TrackReader tr;
    std::vector<uint8_t> data(100, 0);
    CHECK(tr.LoadTrackData(data) == true);
}

TEST_CASE("after failed load, ExtractTrackCoordinates returns empty") {
    TrackReader tr;
    tr.LoadTrackData(std::vector<uint8_t>{});
    auto segs = tr.ExtractTrackCoordinates(SR_TRACK_DRAG_RACE);
    CHECK(segs.empty());
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// TrackReader::ExtractTrackCoordinates (with synthetic data)
// ---------------------------------------------------------------------------

TEST_SUITE("TrackReader::ExtractTrackCoordinates") {

TEST_CASE("extracts coordinates from synthetic drag-race offset") {
    // Build one segment: xStart=10, xEnd=20, yStart=5, yEnd=15, sprites=0
    SrTrackCoordSegment expected;
    expected.xStart = 10; expected.xEnd = 20;
    expected.yStart = 5;  expected.yEnd = 15;
    expected.sprite1 = 0; expected.sprite2 = 0;

    auto buf = MakeTrackBinary(SR_TRACK_DRAG_RACE_OFFSET, {expected});

    TrackReader tr;
    tr.LoadTrackData(buf);
    auto segs = tr.ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 1);

    REQUIRE(segs.size() == 1);
    CHECK(segs[0].xStart == expected.xStart);
    CHECK(segs[0].xEnd   == expected.xEnd);
    CHECK(segs[0].yStart == expected.yStart);
    CHECK(segs[0].yEnd   == expected.yEnd);
}

TEST_CASE("extracts sprite IDs correctly") {
    SrTrackCoordSegment seg = {};
    seg.xStart = 0; seg.xEnd = 0; seg.yStart = 0; seg.yEnd = 0;
    seg.sprite1 = 31; seg.sprite2 = 32;

    auto buf = MakeTrackBinary(SR_TRACK_DRAG_RACE_OFFSET, {seg});
    TrackReader tr;
    tr.LoadTrackData(buf);
    auto segs = tr.ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 1);

    REQUIRE(segs.size() == 1);
    CHECK(segs[0].sprite1 == 31);
    CHECK(segs[0].sprite2 == 32);
}

TEST_CASE("stops at maxSegments") {
    std::vector<SrTrackCoordSegment> input(10);
    for (auto& s : input) s = {};

    auto buf = MakeTrackBinary(SR_TRACK_DRAG_RACE_OFFSET, input);
    TrackReader tr;
    tr.LoadTrackData(buf);
    auto segs = tr.ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 3);

    CHECK(segs.size() == 3);
}

TEST_CASE("stops when coordinates are unreasonably large") {
    // First segment is fine, second has extreme coords that trigger the validation check
    SrTrackCoordSegment good = {}, bad = {};
    good.xStart = 5; good.xEnd = 10; good.yStart = 3; good.yEnd = 8;
    // Coordinates are already scaled by MakeTrackBinary, so 500*16 = 8000 > 400 limit
    bad.xStart = 500; bad.xEnd = 0; bad.yStart = 0; bad.yEnd = 0;

    auto buf = MakeTrackBinary(SR_TRACK_DRAG_RACE_OFFSET, {good, bad});
    TrackReader tr;
    tr.LoadTrackData(buf);
    auto segs = tr.ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 10);

    // Should stop after the good segment
    CHECK(segs.size() == 1);
    CHECK(segs[0].xStart == 5);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// TrackReader::AnalyzeTrack
// ---------------------------------------------------------------------------

TEST_SUITE("TrackReader::AnalyzeTrack") {

TEST_CASE("empty input returns zeroed analysis") {
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({});
    CHECK(analysis.segmentCount == 0);
    CHECK(analysis.curvedSegments == 0);
    CHECK(analysis.straightSegments == 0);
    CHECK(analysis.totalLength == doctest::Approx(0.0f));
    CHECK(analysis.uniqueSprites.empty());
}

TEST_CASE("segment count matches input") {
    std::vector<SrTrackCoordSegment> segs(5);
    for (auto& s : segs) s = {};
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack(segs);
    CHECK(analysis.segmentCount == 5);
}

TEST_CASE("straight segment (no displacement) counted correctly") {
    SrTrackCoordSegment seg = {};
    seg.xStart = 10; seg.xEnd = 10; // no X movement
    seg.yStart =  5; seg.yEnd =  5; // no Y movement
    seg.sprite1 = 0; seg.sprite2 = 0;
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({seg});
    CHECK(analysis.straightSegments == 1);
    CHECK(analysis.curvedSegments   == 0);
}

TEST_CASE("moving segment counted as curved") {
    SrTrackCoordSegment seg = {};
    seg.xStart = 0; seg.xEnd = 3;
    seg.yStart = 0; seg.yEnd = 4;
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({seg});
    CHECK(analysis.curvedSegments   == 1);
    CHECK(analysis.straightSegments == 0);
}

TEST_CASE("segment length is Euclidean distance") {
    SrTrackCoordSegment seg = {};
    seg.xStart = 0; seg.xEnd = 3;
    seg.yStart = 0; seg.yEnd = 4; // 3-4-5 triangle
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({seg});
    CHECK(analysis.totalLength == doctest::Approx(5.0f));
}

TEST_CASE("bounding box tracks min/max of all segments") {
    SrTrackCoordSegment s1 = {}, s2 = {};
    s1.xStart = -5; s1.xEnd =  0; s1.yStart = 0; s1.yEnd = 10;
    s2.xStart = 10; s2.xEnd = 20; s2.yStart = -3; s2.yEnd = 5;
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({s1, s2});
    CHECK(analysis.minX == -5);
    CHECK(analysis.maxX == 20);
    CHECK(analysis.minY == -3);
    CHECK(analysis.maxY == 10);
}

TEST_CASE("unique sprite IDs are collected (zeros excluded)") {
    SrTrackCoordSegment s1 = {}, s2 = {};
    s1.sprite1 = 31; s1.sprite2 = 0;   // 0 should be excluded
    s2.sprite1 = 32; s2.sprite2 = 31;  // 31 appears twice, should only count once
    TrackReader tr;
    auto analysis = tr.AnalyzeTrack({s1, s2});
    REQUIRE(analysis.uniqueSprites.size() == 2);
    // Check 31 and 32 are present (sorted order)
    CHECK(analysis.uniqueSprites[0] == 31);
    CHECK(analysis.uniqueSprites[1] == 32);
}

} // TEST_SUITE
