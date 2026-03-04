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
// Manifolds
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager manifolds") {

TEST_CASE("GM V6: stock and performance manifolds fit") {
    // MANIFOLD_STOCK   = GM 2-brl manifold
    // MANIFOLD_PERFORMANCE = GM manifold for 3-2brl carbs
    CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_STOCK,       CarEngineGM_V6));
    CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_PERFORMANCE, CarEngineGM_V6));
}

TEST_CASE("GM V6: racing/4-brl manifold does not fit") {
    CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_RACING, CarEngineGM_V6));
}

TEST_CASE("GM V6: returns exactly 2 compatible manifolds") {
    auto m = PartsCompatibilityManager::GetCompatibleManifolds(CarEngineGM_V6);
    CHECK(m.size() == 2);
    CHECK(Contains(m, MANIFOLD_STOCK));
    CHECK(Contains(m, MANIFOLD_PERFORMANCE));
    CHECK_FALSE(Contains(m, MANIFOLD_RACING));
}

TEST_CASE("V8 engines: stock and racing manifolds fit") {
    const CarEngine_t v8s[] = {
        CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : v8s) {
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_STOCK,  eng));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_RACING, eng));
    }
}

TEST_CASE("V8 engines: 3-2brl manifold does not fit") {
    const CarEngine_t v8s[] = {
        CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : v8s) {
        CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_PERFORMANCE, eng));
    }
}

TEST_CASE("V8 engines: returns exactly 2 compatible manifolds") {
    auto m = PartsCompatibilityManager::GetCompatibleManifolds(CarEngineGM_V8_327);
    CHECK(m.size() == 2);
    CHECK(Contains(m, MANIFOLD_STOCK));
    CHECK(Contains(m, MANIFOLD_RACING));
    CHECK_FALSE(Contains(m, MANIFOLD_PERFORMANCE));
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Carburetors – engine-level (carb type follows V6 vs V8)
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager carburetors (engine)") {

TEST_CASE("V6 engines: only 2-brl carb fits") {
    const CarEngine_t v6s[] = {CarEngineGM_V6, CarEngineFord_V6, CarEngineChrysler_V6};
    for (CarEngine_t eng : v6s) {
        auto c = PartsCompatibilityManager::GetCompatibleCarburetors(eng);
        CHECK(c.size() == 1);
        CHECK(Contains(c, CARB_2BARREL));
        CHECK_FALSE(Contains(c, CARB_4BARREL));
        CHECK_FALSE(Contains(c, CARB_4BARREL_RACING));
    }
}

TEST_CASE("V8 engines: 4-brl and racing carbs fit, not 2-brl") {
    const CarEngine_t v8s[] = {
        CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : v8s) {
        auto c = PartsCompatibilityManager::GetCompatibleCarburetors(eng);
        CHECK(c.size() == 2);
        CHECK(Contains(c, CARB_4BARREL));
        CHECK(Contains(c, CARB_4BARREL_RACING));
        CHECK_FALSE(Contains(c, CARB_2BARREL));
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Carburetors – manifold-level pairing
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager carburetors (manifold)") {

TEST_CASE("2-brl manifold (STOCK) takes only 2-brl carb") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_STOCK);
    CHECK(c.size() == 1);
    CHECK(Contains(c, CARB_2BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL_RACING));
}

TEST_CASE("3-2brl manifold (PERFORMANCE) takes only 2-brl carbs") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_PERFORMANCE);
    CHECK(c.size() == 1);
    CHECK(Contains(c, CARB_2BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL_RACING));
}

TEST_CASE("racing manifold takes 4-brl and racing carbs, not 2-brl") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_RACING);
    CHECK(c.size() == 2);
    CHECK(Contains(c, CARB_4BARREL));
    CHECK(Contains(c, CARB_4BARREL_RACING));
    CHECK_FALSE(Contains(c, CARB_2BARREL));
}

TEST_CASE("4-brl and racing carbs are interchangeable on racing manifold") {
    // Racing manifold & carbs have the same fitting as 4-brl manifolds and carbs
    CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL,        MANIFOLD_RACING));
    CHECK(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_RACING));
}

TEST_CASE("2-brl carb does not fit racing manifold") {
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_2BARREL, MANIFOLD_RACING));
}

TEST_CASE("4-brl carb does not fit 2-brl or 3-2brl manifolds") {
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL,        MANIFOLD_STOCK));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL,        MANIFOLD_PERFORMANCE));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_STOCK));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_PERFORMANCE));
}

} // TEST_SUITE
