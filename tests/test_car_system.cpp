#include <doctest/doctest.h>
#include "car_system.h"
#include <algorithm>

template<typename T>
static bool Contains(const std::vector<T>& vec, T val) {
    return std::find(vec.begin(), vec.end(), val) != vec.end();
}

// ---------------------------------------------------------------------------
// GetCompatibleTransmissions – count and contents
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager::GetCompatibleTransmissions") {

// FIXME: pretty sure these are wrong

TEST_CASE("GM V6: 3 GM transmissions, no racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineGM_V6);
    CHECK(t.size() == 3);
    CHECK(Contains(t, TRANS_AUTO_GM));
    CHECK(Contains(t, TRANS_3SPD_GM));
    CHECK(Contains(t, TRANS_4SPD_GM));
    CHECK_FALSE(Contains(t, TRANS_RACING_GM));
}

TEST_CASE("GM V8 282: 4 GM transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineGM_V8_282);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_AUTO_GM));
    CHECK(Contains(t, TRANS_3SPD_GM));
    CHECK(Contains(t, TRANS_4SPD_GM));
    CHECK(Contains(t, TRANS_RACING_GM));
}

TEST_CASE("GM V8 327: 4 GM transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineGM_V8_327);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_RACING_GM));
}

TEST_CASE("Ford V6: 3 Ford transmissions, no racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineFord_V6);
    CHECK(t.size() == 3);
    CHECK(Contains(t, TRANS_AUTO_FORD));
    CHECK(Contains(t, TRANS_3SPD_FORD));
    CHECK(Contains(t, TRANS_4SPD_FORD));
    CHECK_FALSE(Contains(t, TRANS_RACING_FORD));
}

TEST_CASE("Ford V8 292: 4 Ford transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineFord_V8_292);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_RACING_FORD));
}

TEST_CASE("Ford V8 351: 4 Ford transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineFord_V8_351);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_RACING_FORD));
}

TEST_CASE("Chrysler V6: 3 Chrysler transmissions, no racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineChrysler_V6);
    CHECK(t.size() == 3);
    CHECK(Contains(t, TRANS_AUTO_CHRYSLER));
    CHECK(Contains(t, TRANS_3SPD_CHRYSLER));
    CHECK(Contains(t, TRANS_4SPD_CHRYSLER));
    CHECK_FALSE(Contains(t, TRANS_RACING_CHRYSLER));
}

TEST_CASE("Chrysler V8 285: 4 Chrysler transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineChrysler_V8_285);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_RACING_CHRYSLER));
}

TEST_CASE("Chrysler V8 340: 4 Chrysler transmissions including racing") {
    auto t = PartsCompatibilityManager::GetCompatibleTransmissions(CarEngineChrysler_V8_340);
    CHECK(t.size() == 4);
    CHECK(Contains(t, TRANS_RACING_CHRYSLER));
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// IsTransmissionCompatible – cross-vendor rejection and same-vendor acceptance
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager::IsTransmissionCompatible") {

TEST_CASE("GM engine accepts GM auto transmission") {
    CHECK(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_AUTO_GM, CarEngineGM_V8_327));
}

TEST_CASE("GM engine accepts GM racing transmission") {
    CHECK(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_GM, CarEngineGM_V8_327));
}

TEST_CASE("GM engine rejects Ford transmissions") {
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_AUTO_FORD,   CarEngineGM_V6));
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_FORD, CarEngineGM_V8_327));
}

TEST_CASE("Ford engine rejects GM transmissions") {
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_AUTO_GM,   CarEngineFord_V8_351));
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_GM, CarEngineFord_V8_292));
}

TEST_CASE("Chrysler engine rejects GM transmission") {
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_AUTO_GM, CarEngineChrysler_V8_340));
}

TEST_CASE("V6 engines reject racing transmission of own vendor") {
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_GM,       CarEngineGM_V6));
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_FORD,     CarEngineFord_V6));
    CHECK_FALSE(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_CHRYSLER, CarEngineChrysler_V6));
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Manifolds – universal compatibility
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager manifolds") {

TEST_CASE("all three manifolds returned for GM V6") {
    auto m = PartsCompatibilityManager::GetCompatibleManifolds(CarEngineGM_V6);
    CHECK(m.size() == 3);
    CHECK(Contains(m, MANIFOLD_STOCK));
    CHECK(Contains(m, MANIFOLD_PERFORMANCE));
    CHECK(Contains(m, MANIFOLD_RACING));
}

TEST_CASE("all three manifolds compatible with every engine") {
    const CarEngine_t engines[] = {
        CarEngineGM_V6, CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V6, CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V6, CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : engines) {
        CHECK(PartsCompatibilityManager::GetCompatibleManifolds(eng).size() == 3);
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_RACING, eng));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_STOCK, eng));
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Carburetors – universal compatibility
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager carburetors") {

TEST_CASE("all three carbs returned for Ford V8 351") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(CarEngineFord_V8_351);
    CHECK(c.size() == 3);
    CHECK(Contains(c, CARB_2BARREL));
    CHECK(Contains(c, CARB_4BARREL));
    CHECK(Contains(c, CARB_4BARREL_RACING));
}

TEST_CASE("all three carbs compatible with every engine") {
    const CarEngine_t engines[] = {
        CarEngineGM_V6, CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V6, CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V6, CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : engines) {
        CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, eng));
        CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_2BARREL, eng));
    }
}

} // TEST_SUITE
