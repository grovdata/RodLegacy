#include <doctest/doctest.h>
#include "car_system.h"
#include <cstdlib>

// The original carries three separate carburetor pointers on its car struct
// and the manifold class decides how many are used - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md. Car mirrors that, so
// pulling one carb off a dual quad leaves the other fitted and the car dead.
//
// Car itself pulls in SDL and the whole asset stack, so this pins the shared
// data the model is built on rather than constructing a Car.

TEST_SUITE("Carburetors per manifold") {

TEST_CASE("manifold class order is the binary's, not ManifoldType's") {
    CHECK(ManifoldClass(MANIFOLD_2BARREL) == 0);
    CHECK(ManifoldClass(MANIFOLD_4BARREL) == 1);
    // The two that don't line up: the original has triple at 2, dual quad 3.
    CHECK(ManifoldClass(MANIFOLD_TRIPLE_2BARREL) == 2);
    CHECK(ManifoldClass(MANIFOLD_DUAL_QUAD) == 3);
    CHECK(ManifoldClass(MANIFOLD_RACING) == 4);
}

// Car::GetManifoldType reads a spec's class back through this. Hand-rolling
// the inverse got classes 2 and 3 the wrong way round, which put the V6-only
// triple 2-barrel (and its three carbs) on every big V8 whose spec says dual
// quad - specs 2, 10 and 21 in the shipped table.
TEST_CASE("class to type is the exact inverse of type to class") {
    const ManifoldType all[] = {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_DUAL_QUAD,
                                MANIFOLD_TRIPLE_2BARREL, MANIFOLD_RACING};
    for (ManifoldType m : all) {
        CHECK(ManifoldTypeForClass(ManifoldClass(m)) == m);
    }
    CHECK(ManifoldTypeForClass(2) == MANIFOLD_TRIPLE_2BARREL);
    CHECK(ManifoldTypeForClass(3) == MANIFOLD_DUAL_QUAD);
    // ApplySpec clamps to 0..4, but anything else must not fall off the enum.
    CHECK(ManifoldTypeForClass(-1) == MANIFOLD_2BARREL);
    CHECK(ManifoldTypeForClass(9) == MANIFOLD_2BARREL);
}

TEST_CASE("carb count matches the A4-0x3b9c table 1,1,3,2,2") {
    CHECK(CarburetorsPerManifold(MANIFOLD_2BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_4BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_TRIPLE_2BARREL) == 3);
    CHECK(CarburetorsPerManifold(MANIFOLD_DUAL_QUAD) == 2);
    // Racing runs two, which is why it needs two carbs out of spares.
    CHECK(CarburetorsPerManifold(MANIFOLD_RACING) == 2);
}

TEST_CASE("every manifold fits within the three mounts a car has") {
    const ManifoldType all[] = {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_DUAL_QUAD,
                                MANIFOLD_TRIPLE_2BARREL, MANIFOLD_RACING};
    for (ManifoldType m : all) {
        CHECK(CarburetorsPerManifold(m) >= 1);
        CHECK(CarburetorsPerManifold(m) <= kMaxCarburetors);
    }
}

}  // TEST_SUITE

// The tuning knob drifted apart once already: the save loader mapped it onto
// base 9, the indicator art rests at base 8, and the top of the save's range
// fell off the end. These pin the one mapping everything now shares.
TEST_SUITE("Tuning knob") {

TEST_CASE("the original's whole knob range fits, nothing clamped off") {
    // car+0x2b is signed -8..+4, 0 = perfectly tuned (save_game.md).
    for (int knob = -8; knob <= 4; knob++) {
        int level = kPerfectTuneLevel + knob;
        CHECK(level >= 0);
        CHECK(level <= kTuneLevelCount - 1);
    }
    // And it fills the range exactly - 13 knob values, 13 positions.
    CHECK(kPerfectTuneLevel + -8 == 0);
    CHECK(kPerfectTuneLevel + 4 == kTuneLevelCount - 1);
}

TEST_CASE("perfect is a round trip and scores no penalty") {
    CHECK(kPerfectTuneLevel - kPerfectTuneLevel == 0);
    // CarSetupFromCar's tuneOffset is |level - perfect|, capped at 8, which
    // is exactly |knob| over the original's range.
    CHECK(std::abs(0 - kPerfectTuneLevel) == 8);              // knob -8
    CHECK(std::abs((kTuneLevelCount - 1) - kPerfectTuneLevel) == 4);  // knob +4
}

TEST_CASE("the indicator's resting slot is the perfect position") {
    // engine_bay.cpp draws the knob at 108 + level*2; the tune_knob entry's
    // own rect in the window art is x 124.
    CHECK(108 + kPerfectTuneLevel * 2 == 124);
}

}  // TEST_SUITE
