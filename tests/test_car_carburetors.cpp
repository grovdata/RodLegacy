#include <doctest/doctest.h>
#include "car/car_system.h"
#include <cstdlib>

// Car pulls in SDL, so this pins the shared data the model is built on.

TEST_SUITE("Carburetors per manifold") {

TEST_CASE("manifold class order is the binary's, not ManifoldType's") {
    CHECK(ManifoldClass(MANIFOLD_2BARREL) == 0);
    CHECK(ManifoldClass(MANIFOLD_4BARREL) == 1);
    // The two that don't line up: the original has triple at 2, dual quad 3.
    CHECK(ManifoldClass(MANIFOLD_TRIPLE_2BARREL) == 2);
    CHECK(ManifoldClass(MANIFOLD_DUAL_QUAD) == 3);
    CHECK(ManifoldClass(MANIFOLD_RACING) == 4);
}

// Hand-rolling the inverse once put the triple 2-barrel on big V8s whose spec says dual quad.
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

}

TEST_SUITE("Tuning knob") {

TEST_CASE("the original's whole knob range fits, nothing clamped off") {
    // The save stores the knob signed -8..+4, 0 = perfectly tuned.
    for (int knob = -8; knob <= 4; knob++) {
        int level = kPerfectTuneLevel + knob;
        CHECK(level >= 0);
        CHECK(level <= kTuneLevelCount - 1);
    }
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

}

TEST_CASE("Street Rod 2's extra manifold classes round-trip and take their carburettor counts") {
    const ManifoldType extra[4] = {MANIFOLD_TUNNEL_RAM, MANIFOLD_DUAL_TUNNEL_RAM, MANIFOLD_FUEL_INJECTION,
                                   MANIFOLD_SUPERCHARGER};
    const int carbs[4] = {1, 2, 0, 0};
    for (int i = 0; i < 4; ++i) {
        CHECK(ManifoldClass(extra[i]) == 5 + i);
        CHECK(ManifoldTypeForClass(5 + i) == extra[i]);
        CHECK(CarburetorsPerManifold(extra[i]) == carbs[i]);
    }
}

TEST_CASE("Street Rod 2's extra manifolds fit V8s only and take 4-barrel carbs, or none") {
    for (ManifoldType m : {MANIFOLD_TUNNEL_RAM, MANIFOLD_DUAL_TUNNEL_RAM, MANIFOLD_FUEL_INJECTION,
                           MANIFOLD_SUPERCHARGER}) {
        CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(m, CarEngineGM_V6));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(m, CarEngineFord_V8_292));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(m, CarEngineChrysler_V8_340));
    }
    CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL, MANIFOLD_TUNNEL_RAM));
    CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_DUAL_TUNNEL_RAM));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_2BARREL, MANIFOLD_TUNNEL_RAM));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL, MANIFOLD_FUEL_INJECTION));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL, MANIFOLD_SUPERCHARGER));
}
