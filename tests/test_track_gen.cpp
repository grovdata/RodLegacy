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

    for (std::size_t k = 0; k < GENTLE.size(); ++k) {
        CHECK(track.elev[220 + static_cast<int>(k)] == GENTLE[k]);
        CHECK(track.elev[240 + static_cast<int>(k)] == -GENTLE[k]);
        CHECK(track.elev[270 + static_cast<int>(k)] == GENTLE[k]);
        CHECK(track.elev[290 + static_cast<int>(k)] == -GENTLE[k]);
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
