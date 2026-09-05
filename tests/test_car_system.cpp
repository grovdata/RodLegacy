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

TEST_CASE("all GM engines: 4 GM transmissions including racing") {
    const CarEngine_t gm[] = {CarEngineGM_V6, CarEngineGM_V8_282, CarEngineGM_V8_327};
    for (CarEngine_t eng : gm) {
        auto t = PartsCompatibilityManager::GetCompatibleTransmissions(eng);
        CHECK(t.size() == 4);
        CHECK(Contains(t, TRANS_AUTO_GM));
        CHECK(Contains(t, TRANS_3SPD_GM));
        CHECK(Contains(t, TRANS_4SPD_GM));
        CHECK(Contains(t, TRANS_RACING_GM));
    }
}

TEST_CASE("all Ford engines: 4 Ford transmissions including racing") {
    const CarEngine_t ford[] = {CarEngineFord_V6, CarEngineFord_V8_292, CarEngineFord_V8_351};
    for (CarEngine_t eng : ford) {
        auto t = PartsCompatibilityManager::GetCompatibleTransmissions(eng);
        CHECK(t.size() == 4);
        CHECK(Contains(t, TRANS_AUTO_FORD));
        CHECK(Contains(t, TRANS_3SPD_FORD));
        CHECK(Contains(t, TRANS_4SPD_FORD));
        CHECK(Contains(t, TRANS_RACING_FORD));
    }
}

TEST_CASE("all Chrysler engines: 4 Chrysler transmissions including racing") {
    const CarEngine_t chrysler[] = {CarEngineChrysler_V6, CarEngineChrysler_V8_285, CarEngineChrysler_V8_340};
    for (CarEngine_t eng : chrysler) {
        auto t = PartsCompatibilityManager::GetCompatibleTransmissions(eng);
        CHECK(t.size() == 4);
        CHECK(Contains(t, TRANS_AUTO_CHRYSLER));
        CHECK(Contains(t, TRANS_3SPD_CHRYSLER));
        CHECK(Contains(t, TRANS_4SPD_CHRYSLER));
        CHECK(Contains(t, TRANS_RACING_CHRYSLER));
    }
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

TEST_CASE("V6 engines accept racing transmission of own vendor") {
    CHECK(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_GM,       CarEngineGM_V6));
    CHECK(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_FORD,     CarEngineFord_V6));
    CHECK(PartsCompatibilityManager::IsTransmissionCompatible(TRANS_RACING_CHRYSLER, CarEngineChrysler_V6));
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Manifolds
// ---------------------------------------------------------------------------

TEST_SUITE("PartsCompatibilityManager manifolds") {

// Row 0 of the original's fitment table (A4-0x4672) reads 1 0 1 0 0 over
// 2brl/4brl/triple/dual-quad/racing, so a V6 takes the two 2-barrel
// manifolds and nothing else. See docs/garage/engine_bay_layout.md.
TEST_CASE("GM V6: 2-brl manifold fits, 4-brl does not") {
    CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_2BARREL, CarEngineGM_V6));
    CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_4BARREL, CarEngineGM_V6));
}

TEST_CASE("GM V6: racing/dual-quad manifolds do not fit") {
    CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_RACING, CarEngineGM_V6));
    CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_DUAL_QUAD, CarEngineGM_V6));
}

// Triple-2bbl only fits V6s (confirmed in game: a GM 327 V8 rejects it as an
// install option), not any V8, big-block or small.
TEST_CASE("GM V6: triple-2brl manifold fits") {
    CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_TRIPLE_2BARREL, CarEngineGM_V6));
}

TEST_CASE("GM V6: returns exactly the two 2-barrel manifolds") {
    auto m = PartsCompatibilityManager::GetCompatibleManifolds(CarEngineGM_V6);
    CHECK(m.size() == 2);
    CHECK(Contains(m, MANIFOLD_2BARREL));
    CHECK(Contains(m, MANIFOLD_TRIPLE_2BARREL));
    CHECK_FALSE(Contains(m, MANIFOLD_4BARREL));
    CHECK_FALSE(Contains(m, MANIFOLD_RACING));
}

TEST_CASE("V8 engines: 2-brl/4-brl/dual-quad/racing manifolds fit, triple-2brl does not") {
    const CarEngine_t v8s[] = {
        CarEngineGM_V8_282, CarEngineGM_V8_327,
        CarEngineFord_V8_292, CarEngineFord_V8_351,
        CarEngineChrysler_V8_285, CarEngineChrysler_V8_340
    };
    for (CarEngine_t eng : v8s) {
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_2BARREL, eng));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_4BARREL, eng));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_DUAL_QUAD, eng));
        CHECK_FALSE(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_TRIPLE_2BARREL, eng));
        CHECK(PartsCompatibilityManager::IsManifoldCompatible(MANIFOLD_RACING, eng));
    }
}

TEST_CASE("V8 engines: returns exactly 4 compatible manifolds") {
    auto m = PartsCompatibilityManager::GetCompatibleManifolds(CarEngineGM_V8_327);
    CHECK(m.size() == 4);
    CHECK(Contains(m, MANIFOLD_2BARREL));
    CHECK(Contains(m, MANIFOLD_4BARREL));
    CHECK(Contains(m, MANIFOLD_DUAL_QUAD));
    CHECK_FALSE(Contains(m, MANIFOLD_TRIPLE_2BARREL));
    CHECK(Contains(m, MANIFOLD_RACING));
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
// Fitment here follows the newspaper classifieds' own ad text: manifolds are
// explicitly sold as "for 2-4brl carbs" (dual-quad) or "for 3-2brl carbs"
// (triple-2barrel), not guessed.

TEST_CASE("2-brl manifold takes only 2-brl carb") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_2BARREL);
    CHECK(c.size() == 1);
    CHECK(Contains(c, CARB_2BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL));
    CHECK_FALSE(Contains(c, CARB_4BARREL_RACING));
}

// The original's carb fitment table (A4-0x4663) is 1 0 0 / 0 1 1 / 1 0 0 /
// 0 1 1 / 0 1 1 over manifold classes 2brl, 4brl, triple, dual quad, racing:
// a 2-barrel manifold takes 2-barrels, everything else takes a 4-barrel of
// either kind.
TEST_CASE("4-brl manifold takes either 4-brl carb") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_4BARREL);
    CHECK(c.size() == 2);
    CHECK(Contains(c, CARB_4BARREL));
    CHECK(Contains(c, CARB_4BARREL_RACING));
    CHECK_FALSE(Contains(c, CARB_2BARREL));
}

// "2-4brl carbs" in the classifieds means two 4-barrels, not 2-brl or 4-brl.
TEST_CASE("dual-quad manifold takes 4-brl carbs, not 2-brls") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_DUAL_QUAD);
    CHECK(c.size() == 2);
    CHECK(Contains(c, CARB_4BARREL));
    CHECK(Contains(c, CARB_4BARREL_RACING));
    CHECK_FALSE(Contains(c, CARB_2BARREL));
}

TEST_CASE("triple-2brl manifold (for 3-2brl carbs) takes only 2-brl carbs") {
    auto c = PartsCompatibilityManager::GetCompatibleCarburetors(MANIFOLD_TRIPLE_2BARREL);
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

TEST_CASE("4-brl carb does not fit 2-brl or triple-2brl manifolds") {
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL,        MANIFOLD_2BARREL));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL,        MANIFOLD_TRIPLE_2BARREL));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_2BARREL));
    CHECK_FALSE(PartsCompatibilityManager::IsCarburetorCompatible(CARB_4BARREL_RACING, MANIFOLD_TRIPLE_2BARREL));
}

} // TEST_SUITE
