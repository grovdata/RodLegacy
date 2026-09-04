#include <doctest/doctest.h>
#include "engine_bay_layout.h"

// Pins the positions transcribed from the Amiga binary's tables. The
// class 4 / V8_BIG case is measured pixel-exact off
// RodLegacyResearch/screenshots/010.png - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md.

namespace {

// lib.p natural sizes of the assets each case draws.
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

}  // namespace

TEST_SUITE("Engine bay layout") {

TEST_CASE("engine bodies sit where the binary's table puts them") {
    CHECK(Same(EngineBodyRect(kV6, CarEngineGM_V6), {48, 44, 136, 112}));
    CHECK(Same(EngineBodyRect(kV8, CarEngineFord_V8_292), {48, 44, 152, 112}));
    CHECK(Same(EngineBodyRect(kV8Big, CarEngineChrysler_V8_340), {38, 40, 176, 125}));
}

TEST_CASE("engine ground wire tracks the block") {
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V6), {138, 138, 40, 23}));
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V8_282), {156, 139, 40, 23}));
    // Verified: this wire's last drawn row is y 169 in screenshot 010.png.
    CHECK(Same(EngineWireRect(kWire, CarEngineGM_V8_327), {162, 147, 40, 23}));
}

TEST_CASE("manifold body has one position for every class") {
    CHECK(Same(ManifoldBodyRect(kManifoldWide, MANIFOLD_2BARREL), {80, 42, 112, 34}));
    CHECK(Same(ManifoldBodyRect(kManifoldWide, MANIFOLD_RACING), {80, 42, 112, 34}));
}

TEST_CASE("carb position is decided by the manifold under it") {
    // Single-carb 2-barrel manifold sits lower on a V6 than on a V8.
    CHECK(Same(CarbBodyRect(kCarb2Brl, MANIFOLD_2BARREL, CarEngineGM_V6), {104, 27, 32, 29}));
    CHECK(Same(CarbBodyRect(kCarb2Brl, MANIFOLD_2BARREL, CarEngineGM_V8_327), {111, 22, 32, 29}));
    CHECK(Same(CarbBodyRect(kCarb4Brl, MANIFOLD_4BARREL, CarEngineGM_V6), {104, 13, 40, 38}));
    CHECK(Same(CarbBodyRect(kCarb2Brl, MANIFOLD_TRIPLE_2BARREL, CarEngineGM_V6), {80, 27, 32, 29}));
    CHECK(Same(CarbBodyRect(kCarb4Brl, MANIFOLD_DUAL_QUAD, CarEngineGM_V6), {88, 13, 40, 38}));
    // Verified: this carb's topmost drawn row is y 5 in screenshot 010.png.
    CHECK(Same(CarbBodyRect(kCarbRacing, MANIFOLD_RACING, CarEngineGM_V8_327), {88, 5, 40, 47}));
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

    // Verified: this screw's head is at x 119-124, y 46-49 in screenshot
    // 010.png, i.e. bottom-aligned on y 50.
    SDL_Point carbScrew =
        EngineBayScrewAnchor(3, CarEngineGM_V8_327, MANIFOLD_RACING, CARB_4BARREL_RACING);
    CHECK(carbScrew.x == 118);
    CHECK(carbScrew.y == 50);
}

TEST_CASE("screw hotspot covers the sprite") {
    SDL_Point anchor = {118, 50};
    SDL_Rect zone = EngineBayScrewZone(anchor);
    CHECK(zone.x <= anchor.x);
    CHECK(zone.x + zone.w >= anchor.x + 8);
    CHECK(zone.y <= anchor.y - 13);
    CHECK(zone.y + zone.h >= anchor.y);
}

}  // TEST_SUITE
