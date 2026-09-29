#include <doctest/doctest.h>
#include "car/engine_bay_layout.h"

namespace {

const SDL_Rect kV6 = {0, 0, 136, 112};
const SDL_Rect kV8 = {0, 0, 152, 112};
const SDL_Rect kV8Big = {0, 0, 176, 125};
const SDL_Rect kWire = {0, 0, 40, 23};
const SDL_Rect kCarb2Brl = {0, 0, 32, 29};
const SDL_Rect kCarb4Brl = {0, 0, 40, 38};
const SDL_Rect kCarbRacing = {0, 0, 40, 47};
const SDL_Rect kManifoldWide = {0, 0, 112, 34};

bool Same(SDL_Rect a, SDL_Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

}

TEST_SUITE("Engine bay layout") {

TEST_CASE("engine bodies sit where the binary's table puts them") {
    CHECK(Same(EngineBodyRect(kV6, CarEngineGM_V6), {48, 44, 136, 112}));
    CHECK(Same(EngineBodyRect(kV8, CarEngineFord_V8_292), {48, 44, 152, 112}));
    CHECK(Same(EngineBodyRect(kV8Big, CarEngineChrysler_V8_340), {38, 40, 176, 125}));
}

TEST_CASE("engine ground wire tracks the block") {
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V6), {138, 138, 40, 23}));
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V8_282), {156, 139, 40, 23}));
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V8_327), {162, 147, 40, 23}));
}

TEST_CASE("manifold body has one position for every class") {
    CHECK(Same(ManifoldBodyRect(kManifoldWide, MANIFOLD_2BARREL), {80, 42, 112, 34}));
    CHECK(Same(ManifoldBodyRect(kManifoldWide, MANIFOLD_RACING), {80, 42, 112, 34}));
}

TEST_CASE("the 2-barrel manifold has a narrow body for the V6 block") {
    // A V6 takes only the two 2-barrel manifolds, and both draw 88px wide.
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_2BARREL, CarEngineGM_V6) == true);
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_TRIPLE_2BARREL, CarEngineGM_V6) == true);
    // On a V8 the same part is the 112px body.
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_2BARREL, CarEngineGM_V8_282) == false);
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_2BARREL, CarEngineFord_V8_351) == false);
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_4BARREL, CarEngineGM_V6) == false);
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_RACING, CarEngineGM_V6) == false);
    CHECK(ManifoldBodyIsNarrow(MANIFOLD_TRIPLE_2BARREL, CarEngineGM_V8_327) == true);
}

TEST_CASE("manifold class decides how many carburetors it mounts") {
    CHECK(CarburetorsPerManifold(MANIFOLD_2BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_4BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_TRIPLE_2BARREL) == 3);
    CHECK(CarburetorsPerManifold(MANIFOLD_DUAL_QUAD) == 2);
    CHECK(CarburetorsPerManifold(MANIFOLD_RACING) == 2);
}

TEST_CASE("carb position is decided by the manifold under it") {
    // Single-carb 2-barrel manifold sits lower on a V6 than on a V8.
    CHECK(Same(CarbBodyRect(kCarb2Brl, MANIFOLD_2BARREL, CarEngineGM_V6, 0), {104, 27, 32, 29}));
    CHECK(Same(CarbBodyRect(kCarb2Brl, MANIFOLD_2BARREL, CarEngineGM_V8_327, 0), {111, 22, 32, 29}));
    CHECK(Same(CarbBodyRect(kCarb4Brl, MANIFOLD_4BARREL, CarEngineGM_V6, 0), {104, 13, 40, 38}));
    CHECK(Same(CarbBodyRect(kCarbRacing, MANIFOLD_RACING, CarEngineGM_V8_327, 0), {88, 5, 40, 47}));
    CHECK(Same(CarbBodyRect(kCarbRacing, MANIFOLD_RACING, CarEngineGM_V8_327, 1), {128, 5, 40, 47}));
}

TEST_CASE("multi-carb manifolds space their mounts evenly") {
    // Triple 2-barrel: three carbs, 24px apart, all on one row.
    for (int i = 0; i < 3; i++) {
        SDL_Rect r = CarbBodyRect(kCarb2Brl, MANIFOLD_TRIPLE_2BARREL, CarEngineGM_V6, i);
        CHECK(Same(r, {80 + i * 24, 27, 32, 29}));
    }
    // Dual quad: two 4-barrels, 40px apart.
    CHECK(Same(CarbBodyRect(kCarb4Brl, MANIFOLD_DUAL_QUAD, CarEngineGM_V6, 0), {88, 13, 40, 38}));
    CHECK(Same(CarbBodyRect(kCarb4Brl, MANIFOLD_DUAL_QUAD, CarEngineGM_V6, 1), {128, 13, 40, 38}));
}

TEST_CASE("screws mount on the part they hold") {
    SDL_Point engineScrew = EngineBayScrewAnchor(0, CarEngineGM_V6, MANIFOLD_2BARREL, CARB_2BARREL);
    CHECK(engineScrew.x == 171);
    CHECK(engineScrew.y == 129);

    // Both manifold screws share a row; the narrow triple body pulls the
    // right-hand one in.
    CHECK(EngineBayScrewAnchor(1, CarEngineGM_V6, MANIFOLD_4BARREL, CARB_4BARREL).x == 98);
    CHECK(EngineBayScrewAnchor(2, CarEngineGM_V6, MANIFOLD_4BARREL, CARB_4BARREL).x == 177);
    CHECK(EngineBayScrewAnchor(2, CarEngineGM_V6, MANIFOLD_TRIPLE_2BARREL, CARB_2BARREL).x == 155);
    // Same for the 2-barrel's narrow V6 body, which the V8 keeps wide.
    CHECK(EngineBayScrewAnchor(2, CarEngineGM_V6, MANIFOLD_2BARREL, CARB_2BARREL).x == 155);
    CHECK(EngineBayScrewAnchor(2, CarEngineGM_V8_282, MANIFOLD_2BARREL, CARB_2BARREL).x == 177);

    SDL_Point carbScrew =
        EngineBayScrewAnchor(3, CarEngineGM_V8_327, MANIFOLD_RACING, CARB_4BARREL_RACING);
    CHECK(carbScrew.x == 118);
    CHECK(carbScrew.y == 50);
    // The second carb's screw follows its own mount.
    SDL_Point carbScrew2 =
        EngineBayScrewAnchor(4, CarEngineGM_V8_327, MANIFOLD_RACING, CARB_4BARREL_RACING);
    CHECK(carbScrew2.x == 158);
    CHECK(carbScrew2.y == 50);
}

TEST_CASE("screw hotspot covers the sprite") {
    SDL_Point anchor = {118, 50};
    SDL_Rect zone = EngineBayScrewZone(anchor);
    CHECK(zone.x <= anchor.x);
    CHECK(zone.x + zone.w >= anchor.x + 8);
    CHECK(zone.y <= anchor.y - 13);
    CHECK(zone.y + zone.h >= anchor.y);
}

}
