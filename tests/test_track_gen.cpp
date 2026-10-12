#include <doctest/doctest.h>

#include "race/track_gen.h"

#include <array>
#include <cstdint>

namespace {

using srrace::TRACK_SEGMENTS;
using srrace::TrackData;

constexpr std::array<int, 9> CURVE_SPOTS = {400, 405, 410, 415, 420, 425, 430, 435, 440};
constexpr std::array<int, 11> ELEV_SPOTS = {390, 400, 410, 420, 430, 440, 500, 620, 760, 900, 1089};
constexpr std::array<int16_t, 17> GENTLE = {1, 2, 3, 3, 3, 2, 1, 0, 0, 0, -1, -2, -3, -3, -3, -2, -1};

struct ShapeExpected {
    int64_t elev_sum;
    int64_t elev_checksum;
    int64_t curve_sum;
    int64_t curve_checksum;
    int64_t flags_checksum;
    int64_t left_checksum;
    int64_t right_checksum;
    int16_t final_curve;
    std::array<int16_t, 9> elev_spots;
    std::array<int16_t, 11> curve_spots;
};

constexpr std::array<ShapeExpected, 10> EXPECTED = {{
    {0, -450, -34674, -25795347, 8578848, 4689920, 13533184, -39,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, -12, -42, -72, -102, -132, -135, -15, -24, -159, -39}},
    {0, 774, 13899, 7421772, 8700552, 3307520, 17811456, 9,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, -9, -39, -69, -99, -81, 39, 159, 24, -111, 9}},
    {0, -1596, 139629, 108788892, 8141112, 3688448, 24999936, 264,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 27, 57, 87, 117, 120, 240, 279, 144, 264}},
    {-6, -4233, 34332, 23407887, 7990896, 9121792, 13500416, -39,
     {0, 0, 1, 4, 0, -4, -2, 0, 0}, {0, 0, 0, 0, 0, 0, 120, -9, 105, 123, -39}},
    {0, -618, -45909, -50592642, 9332920, 5494784, 11198464, -294,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 9, 39, 69, 99, 111, -9, 111, -24, -159, -294}},
    {0, -1596, -66390, -47069505, 8631840, 3067904, 35020800, -120,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, -12, -42, -72, -102, -132, -135, -144, -105, 15, -120}},
    {6, 3135, 108453, 84144960, 8876600, 5046272, 11927552, 216,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 27, 57, 87, 117, 120, 240, 195, 105, 216}},
    {0, -522, -124572, -97105287, 7507872, 4382720, 25206784, -246,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, -12, -42, -72, -102, -132, -135, -126, -231, -111, -246}},
    {-6, -1779, -13968, 679992, 6850848, 3067904, 13187072, 96,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, -12, -42, -72, -102, -132, -135, -144, -24, 96, 96}},
    {-12, -8796, 182973, 141206580, 7161112, 5353472, 11927552, 216,
     {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 27, 57, 87, 117, 120, 291, 450, 327, 216}},
}};

int64_t SumElev(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        sum += track.elev[i];
    }
    return sum;
}

int64_t SumCurve(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        sum += track.curve[i];
    }
    return sum;
}

int64_t ChecksumElev(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        sum += static_cast<int64_t>(i + 1) * track.elev[i];
    }
    return sum;
}

int64_t ChecksumCurve(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        sum += static_cast<int64_t>(i + 1) * track.curve[i];
    }
    return sum;
}

int64_t ChecksumFlags(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        sum += static_cast<int64_t>(i + 1) * track.flags[i];
    }
    return sum;
}

int64_t ChecksumLeft(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        sum += static_cast<int64_t>(i + 1) * track.left[i];
    }
    return sum;
}

int64_t ChecksumRight(const TrackData& track) {
    int64_t sum = 0;
    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        sum += static_cast<int64_t>(i + 1) * track.right[i];
    }
    return sum;
}

}

TEST_SUITE("track generation") {

TEST_CASE("GenerateTrackShapes matches Python simulator ground truth") {
    for (int variant = 0; variant < 10; ++variant) {
        TrackData track = {};
        srrace::GenerateTrackShapes(track, variant);

        const ShapeExpected& expected = EXPECTED[variant];
        CAPTURE(variant);
        CHECK(SumElev(track) == expected.elev_sum);
        CHECK(ChecksumElev(track) == expected.elev_checksum);
        CHECK(SumCurve(track) == expected.curve_sum);
        CHECK(ChecksumCurve(track) == expected.curve_checksum);
        CHECK(ChecksumFlags(track) == expected.flags_checksum);
        CHECK(ChecksumLeft(track) == expected.left_checksum);
        CHECK(ChecksumRight(track) == expected.right_checksum);
        CHECK(track.curve[1089] == expected.final_curve);

        for (std::size_t i = 0; i < CURVE_SPOTS.size(); ++i) {
            CHECK(track.elev[CURVE_SPOTS[i]] == expected.elev_spots[i]);
        }
        for (std::size_t i = 0; i < ELEV_SPOTS.size(); ++i) {
            CHECK(track.curve[ELEV_SPOTS[i]] == expected.curve_spots[i]);
        }
    }
}

TEST_CASE("RoadLayoutInit builds city, countryside, finish, and S-curve structure") {
    TrackData track = {};
    srrace::RaceRng rng(0x12345678u);
    srrace::RoadLayoutInit(track, rng, 1);

    for (int i = 0; i < 180; ++i) {
        CHECK((track.flags[i] & 0x0001) != 0);
    }
    for (int i = 180; i < TRACK_SEGMENTS; ++i) {
        CHECK((track.flags[i] & 0x0001) == 0);
    }

    CHECK((track.flags[379] & 0x1010) == 0x1010);
    CHECK((track.flags[1019] & 0x2010) == 0x2010);
    CHECK((track.left[379] & static_cast<uint16_t>(~0x1801u)) == 0);
    CHECK((track.right[379] & static_cast<uint16_t>(~0x1801u)) == 0);
    CHECK((track.left[1019] & static_cast<uint16_t>(~0x1801u)) == 0);
    CHECK((track.right[1019] & static_cast<uint16_t>(~0x1801u)) == 0);

    for (int i = 184; i < TRACK_SEGMENTS; i += 8) {
        CHECK((track.left[i] & 0x0001) != 0);
        CHECK((track.right[i] & 0x0001) != 0);
    }

    // Two blocks, not four: the cursor runs through each one.
    for (std::size_t k = 0; k < GENTLE.size(); ++k) {
        CHECK(track.elev[220 + static_cast<int>(k)] == GENTLE[k]);
        CHECK(track.elev[257 + static_cast<int>(k)] == -GENTLE[k]);
    }
    for (int i = 237; i < 257; ++i) {
        CHECK(track.elev[i] == 0);
    }
    for (int i = 274; i < 380; ++i) {
        CHECK(track.elev[i] == 0);
    }

    CHECK((track.left[29] & 0x1000) != 0);
    CHECK((track.left[24] & 0x0800) != 0);
    CHECK((track.right[44] & 0x0001) != 0);
    CHECK((track.left[108] & 0x0001) != 0);
    CHECK((track.left[87] & 0x1000) != 0);
    CHECK((track.left[82] & 0x0800) != 0);
    CHECK((track.left[156] & 0x0001) != 0);
    CHECK((track.left[135] & 0x1000) != 0);
    CHECK((track.left[130] & 0x0800) != 0);
}

// The re-clear uses its own masks so arches and scenery from RoadLayoutInit survive.
TEST_CASE("GenerateTrackShapes re-clear keeps arch and scenery bits") {
    TrackData stale = {};
    TrackData masked = {};
    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        stale.left[i] = 0xffff;
        stale.right[i] = 0xffff;
        stale.flags[i] = 0xffff;
        stale.elev[i] = 123;
        stale.curve[i] = -321;

        masked.left[i] = 0xe7ff;
        masked.right[i] = 0x87ff;
        masked.flags[i] = 0xf117;
    }

    srrace::GenerateTrackShapes(stale, 5);
    srrace::GenerateTrackShapes(masked, 5);

    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        CAPTURE(i);
        CHECK(stale.left[i] == masked.left[i]);
        CHECK(stale.right[i] == masked.right[i]);
        CHECK(stale.flags[i] == masked.flags[i]);
        CHECK(stale.elev[i] == masked.elev[i]);
        CHECK(stale.curve[i] == masked.curve[i]);
    }
}

// The regression the two-arch report came from: zeroing flags[] in the re-clear
// wiped the "County line" finish gantry and both random mid-course arches.
TEST_CASE("Road race keeps its finish gantry and mid-course arches") {
    for (int seed = 1; seed <= 20; ++seed) {
        CAPTURE(seed);
        TrackData track = {};
        srrace::RaceRng rng(static_cast<uint32_t>(seed) * 0x9e3779b9u);
        srrace::RoadLayoutInit(track, rng, 1);
        srrace::GenerateTrackShapes(track, seed % 10);

        CHECK((track.flags[1019] & 0x2000) != 0);
        CHECK((track.flags[379] & 0x1000) != 0);

        int cityStraight = 0;
        for (int i = 200; i < 380; ++i) {
            if ((track.flags[i] & 0xf000) != 0) ++cityStraight;
        }
        CHECK(cityStraight >= 2);  // the random 0x8000 plus the 379 gantry
    }
}

}

// ---------------------------------------------------------------------------
// Street Rod 2 edited this generator rather than replacing it.
// ---------------------------------------------------------------------------

TEST_CASE("the generator knobs are the documented ones") {
    const srrace::TrackGenParams& sr1 = srrace::TrackParams(srrace::TrackGenSource::StreetRod);
    CHECK(sr1.segments == 1120);
    CHECK(sr1.genLimit == 0x442);
    CHECK(sr1.shapesPerRun == 12);
    CHECK(sr1.curveStep == 3);
    CHECK(sr1.objectMask == 0x1801);
    CHECK_FALSE(sr1.hillsAreCurves);

    const srrace::TrackGenParams& sr2 = srrace::TrackParams(srrace::TrackGenSource::StreetRod2);
    CHECK(sr2.segments == 990);
    CHECK(sr2.genLimit == 0x3de);
    CHECK(sr2.shapesPerRun == 13);
    CHECK(sr2.curveStep == 7);
    CHECK(sr2.objectMask == 0x9811);   // keeps SOLID 0x10 and 0x8000
    CHECK(sr2.hillsAreCurves);

    // Street Rod 2's track is shorter, so it fits the shared arrays.
    CHECK(sr2.segments <= srrace::TRACK_SEGMENTS);
}

TEST_CASE("SR2 turns the hill codes into curves") {
    // Variant 2's row is byte-identical in both games: 1 5 1 3 8 11 6 7 2 4 9 1.
    srrace::TrackData sr1{};
    srrace::GenerateTrackShapes(sr1, 2, srrace::TrackGenSource::StreetRod);
    srrace::TrackData sr2{};
    srrace::GenerateTrackShapes(sr2, 2, srrace::TrackGenSource::StreetRod2);

    int sr1Markers = 0, sr2Markers = 0;
    for (int i = 380; i < 990; ++i) {
        if (sr1.flags[i] & 0x0600) ++sr1Markers;
        if (sr2.flags[i] & 0x0600) ++sr2Markers;
    }
    CHECK(sr1Markers > 0);
    CHECK(sr2Markers > sr1Markers);
}

TEST_CASE("SR2's elevation comes from the rolling profile, not the shapes") {
    // Street Rod's elevation is the shape tables: bumps with flat road between them.
    srrace::TrackData sr1{};
    srrace::GenerateTrackShapes(sr1, 2, srrace::TrackGenSource::StreetRod);
    srrace::TrackData sr2{};
    srrace::GenerateTrackShapes(sr2, 2, srrace::TrackGenSource::StreetRod2);

    int sr1Elevated = 0, sr2Elevated = 0;
    for (int i = 380; i < 990; ++i) {
        if (sr1.elev[i] != 0) ++sr1Elevated;
        if (sr2.elev[i] != 0) ++sr2Elevated;
    }
    CHECK(sr1Elevated > 0);
    CHECK(sr2Elevated > 3 * sr1Elevated);

    // Cumulative, not a repeating table: the road wanders well outside the
    // +-5 the shape tables ever reach.
    int lowest = 0, highest = 0;
    for (int i = 380; i < 990; ++i) {
        lowest = std::min(lowest, int(sr2.elev[i]));
        highest = std::max(highest, int(sr2.elev[i]));
    }
    CHECK(highest - lowest > 20);

    // The run-in to the finish is held flat whatever the table says, so the
    // road does not tilt under the finish line.
    for (int i = 921; i <= 969; ++i) {
        CHECK(sr2.elev[i] == sr2.elev[i - 1]);
    }
}

TEST_CASE("the curve step is what makes SR2 turn harder") {
    int sr1Max = 0, sr2Max = 0;
    for (int v = 0; v < 10; ++v) {
        srrace::TrackData sr1{};
        srrace::GenerateTrackShapes(sr1, v, srrace::TrackGenSource::StreetRod);
        srrace::TrackData sr2{};
        srrace::GenerateTrackShapes(sr2, v, srrace::TrackGenSource::StreetRod2);
        for (int i = 380; i < 990; ++i) {
            sr1Max = std::max(sr1Max, std::abs(int(sr1.curve[i])));
            sr2Max = std::max(sr2Max, std::abs(int(sr2.curve[i])));
        }
    }
    CHECK(sr2Max > sr1Max);
}

TEST_CASE("SR2 writes nothing past its own shape limit") {
    // 0x3de, not Street Rod's 0x442. Writing past it would put curve data in
    // segments Street Rod 2 never draws.
    srrace::TrackData t{};
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) t.curve[i] = 999;
    srrace::GenerateTrackShapes(t, 0, srrace::TrackGenSource::StreetRod2);
    for (int i = 990; i < srrace::TRACK_SEGMENTS; ++i) {
        CHECK(t.curve[i] == 999);
    }
}

TEST_CASE("the variant tables are read from the executable") {
    // 10 rows of big-endian words at a known offset in each game's file.
    const size_t at = 0x2C3B2;   // Street Rod's
    std::vector<uint8_t> exe(at + 10 * 12 * 2 + 16, 0);
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 12; ++c) {
            // Every row all 9s: a legal code, and not what either game ships.
            exe[at + (size_t(r) * 12 + c) * 2 + 1] = 9;
        }
    }
    REQUIRE(srrace::LoadTrackTables(exe, srrace::TrackGenSource::StreetRod));

    srrace::TrackData all9{};
    srrace::GenerateTrackShapes(all9, 0, srrace::TrackGenSource::StreetRod);
    int hills = 0;
    for (int i = 380; i < 1120; ++i) if (all9.elev[i] != 0) ++hills;
    CHECK(hills == 0);           // shape 9 is a narrowing, not a hill

    // A code outside 1..11 is not the table, so the fallbacks stay.
    std::vector<uint8_t> bad = exe;
    bad[at + 1] = 12;
    CHECK_FALSE(srrace::LoadTrackTables(bad, srrace::TrackGenSource::StreetRod));
    std::vector<uint8_t> shortExe(at, 0);
    CHECK_FALSE(srrace::LoadTrackTables(shortExe, srrace::TrackGenSource::StreetRod));

    // Put the shipped table back: later cases measure what it generates.
    for (int r = 0; r < 10; ++r) {
        static const int kRows[10][12] = {
            {2, 3, 1, 7, 4, 10, 5, 8, 6, 2, 9, 1},
            {11, 4, 8, 1, 6, 9, 2, 5, 7, 2, 3, 1},
            {1, 5, 1, 3, 8, 11, 6, 7, 2, 4, 9, 1},
            {3, 1, 4, 9, 2, 6, 8, 1, 3, 8, 10, 2},
            {10, 9, 5, 1, 6, 2, 3, 7, 4, 2, 8, 2},
            {2, 3, 8, 10, 5, 11, 4, 7, 1, 6, 9, 2},
            {1, 5, 9, 1, 4, 2, 3, 10, 8, 4, 1, 9},
            {2, 4, 7, 11, 2, 5, 8, 4, 1, 7, 6, 2},
            {2, 6, 8, 10, 6, 1, 3, 7, 5, 1, 4, 9},
            {1, 3, 1, 6, 1, 4, 9, 10, 6, 8, 2, 3},
        };
        for (int c = 0; c < 12; ++c) {
            const size_t o = at + (size_t(r) * 12 + c) * 2;
            exe[o] = uint8_t(kRows[r][c] >> 8);
            exe[o + 1] = uint8_t(kRows[r][c] & 0xFF);
        }
    }
    REQUIRE(srrace::LoadTrackTables(exe, srrace::TrackGenSource::StreetRod));
}

TEST_CASE("the hill profile is read from the executable") {
    // 61 signed big-endian words. Each nudges a block of ten by one or two,
    // so anything larger is taken as evidence this is not the table.
    const size_t variantsAt = 0x3FF02, hillsAt = 0x40006;
    std::vector<uint8_t> exe(hillsAt + 61 * 2 + 16, 0);
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 13; ++c) {
            exe[variantsAt + (size_t(r) * 13 + c) * 2 + 1] = 9;  // a legal code
        }
    }
    for (int i = 0; i < 61; ++i) exe[hillsAt + size_t(i) * 2 + 1] = 1;  // every block +1
    REQUIRE(srrace::LoadTrackTables(exe, srrace::TrackGenSource::StreetRod2));

    srrace::TrackData t{};
    srrace::GenerateTrackShapes(t, 0, srrace::TrackGenSource::StreetRod2);
    // A constant +1 per segment climbs steadily, except where it is held flat.
    CHECK(t.elev[389] > t.elev[380]);
    CHECK(t.elev[900] > t.elev[500]);

    // A delta outside the range leaves the shipped profile in place.
    std::vector<uint8_t> tooSteep = exe;
    tooSteep[hillsAt + 1] = 40;
    CHECK_FALSE(srrace::LoadTrackTables(tooSteep, srrace::TrackGenSource::StreetRod2));
    // Long enough for the variants but not the profile behind them.
    std::vector<uint8_t> truncated(hillsAt + 8, 0);
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 13; ++c) {
            truncated[variantsAt + (size_t(r) * 13 + c) * 2 + 1] = 9;
        }
    }
    CHECK_FALSE(srrace::LoadTrackTables(truncated, srrace::TrackGenSource::StreetRod2));

    // Put the shipped tables back for the tests after this one.
    static const int kRows[10][13] = {
        {2, 3, 1, 4, 7, 10, 5, 8, 6, 2, 9, 1, 2}, {11, 4, 1, 6, 8, 9, 2, 5, 7, 2, 3, 1, 2},
        {1, 5, 1, 3, 8, 11, 6, 7, 2, 4, 9, 1, 2}, {3, 1, 4, 9, 2, 8, 6, 1, 3, 8, 10, 2, 1},
        {9, 10, 5, 1, 6, 2, 3, 7, 4, 2, 8, 2, 1}, {2, 3, 8, 10, 5, 11, 4, 7, 1, 6, 9, 2, 1},
        {1, 5, 9, 1, 4, 2, 3, 10, 8, 4, 1, 9, 1}, {2, 4, 7, 11, 2, 5, 8, 4, 1, 7, 6, 2, 1},
        {2, 6, 8, 10, 6, 1, 3, 7, 5, 1, 4, 9, 2}, {1, 3, 1, 6, 1, 4, 9, 10, 6, 8, 2, 3, 1},
    };
    static const int kHills[61] = {1, -2, 2, -2, 2, -2, 2, -1, 0, 1, 0, 1, 0, -1, 0, 1, 0, 1, -1, 2,
                                   1, 0, 1, -1, 2, -1, 0, 1, -1, 1, 1, -1, 0, -1, 1, 0, 1, 2, -1, 0,
                                   0, 1, -1, 2, -1, 2, -1, 0, 1, 1, 0, -1, 0, 1, 1, -1, 0, -1, 1, 0, 1};
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 13; ++c) exe[variantsAt + (size_t(r) * 13 + c) * 2 + 1] = uint8_t(kRows[r][c]);
    }
    for (int i = 0; i < 61; ++i) {
        exe[hillsAt + size_t(i) * 2] = uint8_t(uint16_t(kHills[i]) >> 8);
        exe[hillsAt + size_t(i) * 2 + 1] = uint8_t(uint16_t(kHills[i]) & 0xFF);
    }
    REQUIRE(srrace::LoadTrackTables(exe, srrace::TrackGenSource::StreetRod2));
}

TEST_CASE("Street Rod 2 scatters rocks and oil, Street Rod scatters none") {
    // Both hazards live only past segment 400, inside the Mulholland window.
    srrace::TrackData sr1{};
    srrace::RaceRng rng1(12345);
    srrace::ScatterHazards(sr1, rng1, srrace::TrackGenSource::StreetRod);
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        CHECK((sr1.flags[i] & 0x1004) == 0);
        CHECK(sr1.lane[i] == 0);
    }

    // Over several seeds, so the comparison below is about the odds and not one RNG stream.
    int rocks = 0, oil = 0;
    for (uint32_t seed = 12345; seed < 12345 + 20; ++seed) {
    srrace::TrackData sr2{};
    srrace::RaceRng rng2(seed);
    srrace::ScatterHazards(sr2, rng2, srrace::TrackGenSource::StreetRod2);
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        const bool isRock = (sr2.flags[i] & 0x0004) != 0;
        const bool isOil = (sr2.flags[i] & 0x1000) != 0;
        if (isRock) ++rocks;
        if (isOil) ++oil;
        // Never both on one segment, and never outside the window.
        if (isRock) CHECK_FALSE(isOil);
        if (isRock || isOil) {
            CHECK(i > 400);
            CHECK(i < 990);
            // A hazard always names a lane across the road.
            CHECK(sr2.lane[i] >= 1);
            CHECK(sr2.lane[i] <= 16);
        } else {
            CHECK(sr2.lane[i] == 0);
        }
    }
    }
    // Oil is the commoner of the two: 1 in 80 every segment, against a rock's
    // 1 in 64 that also has to clear a 70-segment gap.
    CHECK(oil > rocks);
    CHECK(oil > 0);
}

TEST_CASE("rocks keep their distance") {
    // Every seed has to respect the gap, not just a lucky one.
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        srrace::TrackData t{};
        srrace::RaceRng rng(seed * 7919u);
        srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);
        int previous = -1;
        for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
            if ((t.flags[i] & 0x0004) == 0) continue;
            if (previous >= 0) {
                CHECK(i - previous > 1);
            }
            previous = i;
        }
    }
}

TEST_CASE("the shoulder walls step in blocks") {
    // Below the drag window's end both sides are flat; from 380 the walls are
    // driven in blocks and hold their value between them.
    srrace::TrackData t{};
    srrace::RaceRng rng(4242);
    srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);

    for (int i = 0; i < 380; ++i) {
        CHECK(t.wallLeft[i] == 0);
        CHECK(t.wallRight[i] == 0);
    }

    // The blocks starting 380 and 390 drive left up and right down, 200 a segment, through 399.
    CHECK(t.wallLeft[380] == 200);
    CHECK(t.wallRight[380] == -200);
    CHECK(t.wallLeft[399] == 20 * 200);
    CHECK(t.wallRight[399] == -20 * 200);

    // 400..419 reverses it, back to flat.
    CHECK(t.wallLeft[419] == 0);
    CHECK(t.wallRight[419] == 0);
    // And holds there until the next block.
    CHECK(t.wallLeft[499] == 0);
    CHECK(t.wallRight[499] == 0);

    // 500..519 digs the left side down and raises the right.
    CHECK(t.wallLeft[519] == -20 * 200);
    CHECK(t.wallRight[519] == 20 * 200);

    // Street Rod has no walls at all.
    srrace::TrackData sr1{};
    srrace::RaceRng rng1(4242);
    srrace::ScatterHazards(sr1, rng1, srrace::TrackGenSource::StreetRod);
    for (int i = 0; i < srrace::TRACK_SEGMENTS; ++i) {
        CHECK(sr1.wallLeft[i] == 0);
        CHECK(sr1.wallRight[i] == 0);
    }
}

TEST_CASE("a drop on the right is published to the object bits") {
    // Bit 15 is what arms the drop-off crash, so it has to track the sign of
    // the wall rather than being placed separately.
    srrace::TrackData t{};
    srrace::RaceRng rng(99);
    srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);

    int drops = 0;
    for (int i = 0; i < 990; ++i) {
        const bool armed = (t.right[i] & 0x8000) != 0;
        if (t.wallRight[i] < 0) {
            CHECK(armed);
            ++drops;
        }
    }
    CHECK(drops > 0);
}

// ---------------------------------------------------------------------------
// The Aqueduct is hand authored: the same 371 segments every race. Layout in
// RodLegacyResearch/docs/sr2_amiga/races.md, "Aqueduct track data".
// ---------------------------------------------------------------------------

TEST_CASE("the Aqueduct track is the same every time") {
    srrace::AqueductTrack a{};
    srrace::AqueductTrack b{};
    srrace::BuildAqueductTrack(a);
    srrace::BuildAqueductTrack(b);
    for (int s = 0; s < srrace::AQUEDUCT_SEGMENTS; ++s) {
        CHECK(a.flags[s] == b.flags[s]);
    }

    // The mirror is the forward array reversed - the far-half pass walks it
    // from the other end.
    for (int s = 0; s < srrace::AQUEDUCT_SEGMENTS; ++s) {
        CHECK(a.mirror[s] == a.flags[srrace::AQUEDUCT_SEGMENTS - 1 - s]);
    }
}

TEST_CASE("the Aqueduct's obstacles are where they were authored") {
    srrace::AqueductTrack t{};
    srrace::BuildAqueductTrack(t);

    // One box across the channel.
    for (int s : {4, 76, 220, 316, 364}) {
        CHECK((t.flags[s] & srrace::AQU_OBSTACLE_SPAN) != 0);
    }
    // A box at each side.
    for (int s : {124, 268}) {
        CHECK((t.flags[s] & srrace::AQU_OBSTACLE_SIDES) != 0);
    }
    // And nowhere else: the speed cap keys off this mask, so a stray bit
    // would brake the car on an empty stretch.
    int obstacles = 0;
    for (int s = 0; s < srrace::AQUEDUCT_SEGMENTS; ++s) {
        if ((t.flags[s] & srrace::AQU_OBSTACLE_MASK) != 0) ++obstacles;
    }
    CHECK(obstacles == 7);
}

TEST_CASE("the repeating pattern is laid down before the fixups") {
    srrace::AqueductTrack t{};
    srrace::BuildAqueductTrack(t);

    // Every twenty-fourth run of four is assigned outright, so a segment in
    // one carries bit 1 only if a later pass put it there.
    CHECK((t.flags[3] & 0x2) != 0);
    CHECK((t.flags[6] & 0x2) != 0);
    CHECK((t.flags[30] & 0x2) != 0);

    // Bit 0 every sixth segment, added after.
    CHECK((t.flags[1] & 0x1) != 0);
    CHECK((t.flags[7] & 0x1) != 0);
    CHECK((t.flags[2] & 0x1) == 0);

    // Two places trade 0x4/0x8 for 0x100/0x200, and keep neither of the old
    // bits.
    for (int s : {137, 281}) {
        CHECK((t.flags[s] & 0x0100) != 0);
        CHECK((t.flags[s] & 0x0004) == 0);
    }
    for (int s : {136, 280}) {
        CHECK((t.flags[s] & 0x0200) != 0);
        CHECK((t.flags[s] & 0x0008) == 0);
    }
    // Everywhere else in that group keeps the original pair.
    CHECK((t.flags[17] & 0x0004) != 0);
    CHECK((t.flags[16] & 0x0008) != 0);
}


// ---------------------------------------------------------------------------
// The town generator is shared; only these two things differ between games.
// ---------------------------------------------------------------------------

TEST_CASE("the cruise buildings are in both games, their flanking objects are not") {
    srrace::TrackData a{}, b{};
    srrace::RaceRng ra(0x1234u), rb(0x1234u);
    srrace::RoadLayoutInit(a, ra, 1, srrace::TrackGenSource::StreetRod);
    srrace::RoadLayoutInit(b, rb, 1, srrace::TrackGenSource::StreetRod2);

    // The three buildings a cruise stops at are laid in both.
    CHECK((a.right[44] & 0x0001) != 0);
    CHECK((b.right[44] & 0x0001) != 0);
    CHECK((a.left[108] & 0x0001) != 0);
    CHECK((b.left[108] & 0x0001) != 0);
    CHECK((a.left[156] & 0x0001) != 0);
    CHECK((b.left[156] & 0x0001) != 0);

    // The six objects flanking them are Street Rod's alone.
    const int flank1000[3] = {29, 87, 135};
    const int flank0800[3] = {24, 82, 130};
    for (int i = 0; i < 3; ++i) {
        CHECK((a.left[flank1000[i]] & 0x1000) != 0);
        CHECK((b.left[flank1000[i]] & 0x1000) == 0);
        CHECK((a.left[flank0800[i]] & 0x0800) != 0);
        CHECK((b.left[flank0800[i]] & 0x0800) == 0);
    }
}

TEST_CASE("a drop on the right takes the building marker with it") {
    srrace::TrackData t{};
    srrace::RaceRng rng(0x2222u);
    srrace::RoadLayoutInit(t, rng, 1, srrace::TrackGenSource::StreetRod2);
    srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);

    bool sawDrop = false;
    for (int s = 0; s < srrace::TRACK_SEGMENTS; ++s) {
        if (t.wallRight[s] >= 0) continue;
        sawDrop = true;
        // The drop is published to the object bits...
        CHECK((t.right[s] & 0x8000) != 0);
        // ...and bit 0 is cleared, so no building stands over the drop.
        CHECK((t.right[s] & 0x0001) == 0);
    }
    CHECK(sawDrop);
}


TEST_CASE("the on-road hazard shapes are a distance ladder") {
    // Two hazards, each sliced into eleven phases - the same 0..10 index the
    // roadside sprites use.
    for (int kind = 0; kind < srrace::HazardKindCount; ++kind) {
        int prevArea = 1 << 30;
        for (int p = 0; p < srrace::HAZARD_PHASES; ++p) {
            const srrace::HazardRect& r = srrace::HazardRectFor(kind, p);
            CHECK(r.w > 0);
            CHECK(r.h > 0);
            const int area = int(r.w) * int(r.h);
            CHECK(area <= prevArea);   // never grows with distance
            prevArea = area;
        }
    }
    // Phase 0 is the nearest, and the oil patch is the wider of the two.
    CHECK(srrace::HazardRectFor(srrace::HazardOil, 0).w >
          srrace::HazardRectFor(srrace::HazardRock, 0).w);

    // Stored one based, so the ids that reach the asset layer are one lower.
    CHECK(srrace::HazardAsset(srrace::HazardRock) == 283);
    CHECK(srrace::HazardAsset(srrace::HazardOil) == 296);

    // Out of range asks give an empty rect rather than reading past the table.
    CHECK(srrace::HazardRectFor(srrace::HazardRock, srrace::HAZARD_PHASES).w == 0);
    CHECK(srrace::HazardRectFor(-1, 0).w == 0);
}

TEST_CASE("a file that does not hold the hazard table is refused") {
    CHECK_FALSE(srrace::LoadHazardShapes(std::vector<uint8_t>(16, 0)));
    CHECK_FALSE(srrace::LoadHazardShapes(std::vector<uint8_t>(0x40000, 0)));
    // The built-in copy still stands afterwards.
    CHECK(srrace::HazardAsset(srrace::HazardRock) == 283);

    // A record whose asset number is not the one we mean is not our record.
    std::vector<uint8_t> exe(0x40000, 0);
    const size_t at = srrace::HAZARD_SHAPE_TABLE_OFFSET +
                      size_t(srrace::HAZARD_ROCK_RECORD) * srrace::HAZARD_RECORD_SIZE;
    exe.resize(at + 4096, 0);
    exe[at] = 0x99;
    exe[at + 1] = 0x99;
    CHECK_FALSE(srrace::LoadHazardShapes(exe));
}


TEST_CASE("the grudge strip is a dashed copy of road segments 191..307") {
    srrace::TrackData road{};
    srrace::RaceRng rng(0x4321u);
    srrace::RoadLayoutInit(road, rng, 1, srrace::TrackGenSource::StreetRod2);
    srrace::GenerateTrackShapes(road, 0, srrace::TrackGenSource::StreetRod2);

    srrace::TrackData strip = road;
    srrace::BuildGrudgeStrip(strip);

    for (int s = 0; s < srrace::GRUDGE_SEGMENTS; ++s) {
        const int from = srrace::GRUDGE_ROAD_FIRST + s;
        // Curve and elevation come across untouched.
        CHECK(strip.curve[s] == road.curve[from]);
        CHECK(strip.elev[s] == road.elev[from]);
        CHECK(strip.left[s] == road.left[from]);
        CHECK(strip.right[s] == road.right[from]);
        // Every segment carries the centre dash, however the road had it.
        CHECK((strip.flags[s] & srrace::GRUDGE_DASH_BIT) != 0);
    }

    // Straight, because the shape pass starts at 390 and the hill profile at 380, both above the copied
    // 191..307.
    for (int s = 0; s < srrace::GRUDGE_SEGMENTS; ++s) {
        CHECK(strip.curve[s] == 0);
        CHECK(std::abs(static_cast<int>(strip.elev[s])) <= 3);
    }
    bool levelStart = true;
    for (int s = 0; s < 29; ++s) {
        if (strip.elev[s] != 0) levelStart = false;
    }
    CHECK(levelStart);
    bool humpedLater = false;
    for (int s = 29; s < srrace::GRUDGE_SEGMENTS; ++s) {
        if (strip.elev[s] != 0) humpedLater = true;
    }
    CHECK(humpedLater);
    const srrace::RaceWindow grudge = srrace::RaceWindowFor(srrace::RaceKind::Grudge);
    const srrace::RaceWindow drag = srrace::RaceWindowFor(srrace::RaceKind::Drag);
    CHECK(srrace::GRUDGE_ROAD_FIRST + grudge.start >= drag.start);
    CHECK(srrace::GRUDGE_ROAD_FIRST + grudge.finish <= drag.finish);

    // The dash colour changes one segment before the finish at 89.
    CHECK((strip.flags[srrace::GRUDGE_FINISH_MARKER] &
           srrace::GRUDGE_DASH_COLOUR_BIT) != 0);

    // The renderer paints that bit as a line across the whole road.
    std::vector<int> roadMarked;
    for (int s = 0; s < srrace::TRACK_SEGMENTS; ++s) {
        if (road.flags[s] & srrace::GRUDGE_DASH_COLOUR_BIT) roadMarked.push_back(s);
    }
    REQUIRE(roadMarked.size() == 2);
    CHECK(roadMarked[0] == drag.finish - 1);
    CHECK(roadMarked[1] == srrace::RaceWindowFor(srrace::RaceKind::Road).finish - 1);
    CHECK((road.flags[roadMarked[0]] & 0xf000) != 0);
    CHECK((road.flags[roadMarked[1]] & 0xf000) != 0);

    // The strip keeps exactly one, its own, because both of the road's sit
    // above the copied 191..307 window.
    int stripMarked = 0;
    for (int s = 0; s < srrace::TRACK_SEGMENTS; ++s) {
        if (strip.flags[s] & srrace::GRUDGE_DASH_COLOUR_BIT) ++stripMarked;
    }
    CHECK(stripMarked == 1);
    CHECK(srrace::GRUDGE_FINISH_MARKER == grudge.finish - 1);
    CHECK(srrace::GRUDGE_FINISH_MARKER <
          srrace::RaceWindowFor(srrace::RaceKind::Grudge).finish);

    // Nothing past the strip: the race window ends well inside it.
    for (int s = srrace::GRUDGE_SEGMENTS; s < srrace::TRACK_SEGMENTS; ++s) {
        CHECK(strip.flags[s] == 0);
        CHECK(strip.curve[s] == 0);
        CHECK(strip.elev[s] == 0);
    }
    CHECK(srrace::RaceWindowFor(srrace::RaceKind::Grudge).finish <
          srrace::GRUDGE_SEGMENTS);
}

TEST_CASE("the roadside scatter replays from its seed") {
    // jroad.c FUN_0024c7ec builds the road once a session and rewinds the RNG to the saved seed on a load.
    auto build = [](uint32_t seed) {
        TrackData t{};
        srrace::RaceRng rng(seed);
        srrace::RoadLayoutInit(t, rng, 1, srrace::TrackGenSource::StreetRod2);
        srrace::GenerateTrackShapes(t, 3, srrace::TrackGenSource::StreetRod2);
        srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);
        return t;
    };

    const TrackData a = build(0x1234u);
    const TrackData b = build(0x1234u);
    const TrackData c = build(0x9999u);

    int same = 0;
    int differs = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        if (a.flags[i] == b.flags[i] && a.left[i] == b.left[i] &&
            a.right[i] == b.right[i] && a.lane[i] == b.lane[i]) {
            ++same;
        }
        if (a.flags[i] != c.flags[i] || a.lane[i] != c.lane[i]) {
            ++differs;
        }
    }
    CHECK(same == TRACK_SEGMENTS);
    CHECK(differs > 0);

    // A seed of 0 is the original's fresh-launch case, and RaceRng maps it to 1
    // rather than degenerating, so it still lays a road.
    const TrackData zero = build(0u);
    int built = 0;
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        if (zero.flags[i] != 0) ++built;
    }
    CHECK(built > 0);
}

TEST_CASE("Street Rod 2 lays five segments of wet cement and piles only under a wall") {
    int cementRuns = 0, piles = 0;
    for (int variant = 0; variant < 10; ++variant) {
        srrace::TrackData t{};
        srrace::RaceRng rng(777 + variant);
        srrace::RoadLayoutInit(t, rng, 1, srrace::TrackGenSource::StreetRod2);
        srrace::GenerateTrackShapes(t, variant, srrace::TrackGenSource::StreetRod2);
        srrace::ScatterHazards(t, rng, srrace::TrackGenSource::StreetRod2);
        for (int i = 380; i < 990; ++i) {
            const uint16_t bit = t.flags[i] & 0x0060;
            if (bit != 0 && (t.flags[i - 1] & bit) == 0) {
                int len = 0;
                while (i + len < 990 && (t.flags[i + len] & bit) != 0) ++len;
                CHECK(len == 5);
                CHECK((t.flags[i] & 0x0008) == 0);   // no dash on cement
                ++cementRuns;
            }
            if (i > 240 && (t.left[i] & 0x0010) != 0) {
                CHECK(t.wallLeft[i + 1] > 0);
                CHECK((t.left[i - 1] & 0x1000) != 0);
                ++piles;
            }
            if (i > 240 && (t.right[i] & 0x0010) != 0) {
                CHECK(t.wallRight[i + 1] > 0);
                CHECK((t.right[i - 1] & 0x1000) != 0);
                ++piles;
            }
        }
    }
    CHECK(cementRuns > 0);
    CHECK(piles > 0);
}
