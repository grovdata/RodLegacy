#include <doctest/doctest.h>
#include "parsers/libp.h"

// ---------------------------------------------------------------------------
// CarQuery
// ---------------------------------------------------------------------------

TEST_SUITE("CarQuery::SrLibQueryCanChopRoof") {

TEST_CASE("Corvettes cannot have roof chopped") {
    CHECK(CarQuery::SrLibQueryCanChopRoof(CHEVROLET_CORVETTE_1956_BODY) == false);
    CHECK(CarQuery::SrLibQueryCanChopRoof(CHEVROLET_CORVETTE_1961_BODY) == false);
    CHECK(CarQuery::SrLibQueryCanChopRoof(CHEVROLET_CORVETTE_1963_BODY) == false);
    CHECK(CarQuery::SrLibQueryCanChopRoof(THE_KING_CHEVROLET_CORVETTE_1963_BODY) == false);
}

TEST_CASE("other cars can have roof chopped") {
    CHECK(CarQuery::SrLibQueryCanChopRoof(CHEVROLET_2DR_COUPE_1940_BODY) == true);
    CHECK(CarQuery::SrLibQueryCanChopRoof(FORD_DELUXE_2DR_1940_BODY)     == true);
    CHECK(CarQuery::SrLibQueryCanChopRoof(DODGE_POLARA_1962_BODY)        == true);
}

} // TEST_SUITE

TEST_SUITE("CarQuery::SrLibQueryCanStripBumpers") {

// Only the three bodies that ship no CHOPPED_REARBUMPER/FRONTBUMPER pair.
TEST_CASE("cars that cannot strip bumpers") {
    CHECK(CarQuery::SrLibQueryCanStripBumpers(FORD_DEUCE_COUPE_1932_BODY)         == false);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(FORD_SEDAN_1932_BODY)               == false);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(THE_KING_CHEVROLET_CORVETTE_1963_BODY) == false);
}

TEST_CASE("standard cars can strip bumpers") {
    CHECK(CarQuery::SrLibQueryCanStripBumpers(CHEVROLET_IMPALA_1958_BODY)        == true);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(DODGE_CUSTOM_ROYAL_LANCER_1955_BODY) == true);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(FORD_FAIRLANE_500_1956_BODY)       == true);
}

// The player Corvettes can, confirmed in game - they just cannot be chopped.
TEST_CASE("Corvettes can strip bumpers") {
    CHECK(CarQuery::SrLibQueryCanStripBumpers(CHEVROLET_CORVETTE_1956_BODY) == true);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(CHEVROLET_CORVETTE_1961_BODY) == true);
    CHECK(CarQuery::SrLibQueryCanStripBumpers(CHEVROLET_CORVETTE_1963_BODY) == true);
}

} // TEST_SUITE

TEST_SUITE("CarQuery::GetChoppedBumperIndex") {

TEST_CASE("rear bumper index is body + 2 for standard cars") {
    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(CHEVROLET_2DR_COUPE_1940_BODY) ==
          CHEVROLET_2DR_COUPE_1940_BODY + 2);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(FORD_DELUXE_2DR_1940_BODY) ==
          FORD_DELUXE_2DR_1940_BODY + 2);
}

// A car's assets run BODY, [CHOPPED_ROOF], CHOPPED_REARBUMPER,
// CHOPPED_FRONTBUMPER, and the chopped-roof slot only exists for bodies that
// can be chopped. The Corvettes cannot, so their bumper overlays sit one index
// earlier than everyone else's.
TEST_CASE("bumper overlays are body + 2/3, or body + 1/2 with no chopped roof") {
    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(CHEVROLET_2DR_COUPE_1940_BODY) ==
          CHEVROLET_2DR_COUPE_1940_CHOPPED_REARBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(CHEVROLET_2DR_COUPE_1940_BODY) ==
          CHEVROLET_2DR_COUPE_1940_CHOPPED_FRONTBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(FORD_DELUXE_2DR_1940_BODY) ==
          FORD_DELUXE_2DR_1940_BODY + 3);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(DODGE_POLARA_1962_BODY) ==
          DODGE_POLARA_1962_BODY + 3);

    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(CHEVROLET_CORVETTE_1956_BODY) ==
          CHEVROLET_CORVETTE_1956_CHOPPED_REARBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(CHEVROLET_CORVETTE_1956_BODY) ==
          CHEVROLET_CORVETTE_1956_CHOPPED_FRONTBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(CHEVROLET_CORVETTE_1961_BODY) ==
          CHEVROLET_CORVETTE_1961_CHOPPED_REARBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(CHEVROLET_CORVETTE_1961_BODY) ==
          CHEVROLET_CORVETTE_1961_CHOPPED_FRONTBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberRearIndex(CHEVROLET_CORVETTE_1963_BODY) ==
          CHEVROLET_CORVETTE_1963_CHOPPED_REARBUMPER);
    CHECK(CarQuery::SrLibQueryGetChoppedBumberFrontIndex(CHEVROLET_CORVETTE_1963_BODY) ==
          CHEVROLET_CORVETTE_1963_CHOPPED_FRONTBUMPER);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// SrLibQueryGetCarIdleX
// ---------------------------------------------------------------------------

TEST_SUITE("SrLibQueryGetCarIdleX") {

TEST_CASE("garage idle X is 35") {
    CHECK(SrLibQueryGetCarIdleX(SCENE_GARAGE) == 35);
}

TEST_CASE("diner idle X is 82") {
    CHECK(SrLibQueryGetCarIdleX(SCENE_DINER) == 82);
}

TEST_CASE("gas station idle X is 40") {
    CHECK(SrLibQueryGetCarIdleX(SCENE_GAS_STATION) == 40);
}

TEST_CASE("unknown scene returns default 35") {
    CHECK(SrLibQueryGetCarIdleX((SrLibGameScene)99) == 35);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// SrLibQueryGetCarRoofYOffset
// ---------------------------------------------------------------------------

TEST_SUITE("SrLibQueryGetCarRoofYOffset") {

// Garage/Diner/GasStation are all bottom-aligned against sprite height -
// live-verified with Copperline, see gas_station.md. No per-car table.
TEST_CASE("known car and height returns the live-verified garage offset") {
    CHECK(SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, 69) == 82);
}

TEST_CASE("same height, different scenes return different offsets") {
    // Corvette 1956 body sprite height (lib.p), see gas_station.md.
    const int height = 56;
    int garage = SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, height);
    int diner  = SrLibQueryGetCarRoofYOffset(SCENE_DINER, height);
    int gas    = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, height);
    CHECK(garage != diner);
    CHECK(garage != gas);
}

TEST_CASE("all three scenes are bottom-aligned to sprite height") {
    CHECK(SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, 69) == 82);
    CHECK(SrLibQueryGetCarRoofYOffset(SCENE_DINER, 69) == 93);
    CHECK(SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, 69) == 112);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// SrLibQueryGetCarVendor
// ---------------------------------------------------------------------------

TEST_SUITE("SrLibQueryGetCarVendor") {

TEST_CASE("Chevrolet cars are GM") {
    CHECK(SrLibQueryGetCarVendor(CHEVROLET_2DR_COUPE_1940)      == CarVendorGM);
    CHECK(SrLibQueryGetCarVendor(CHEVROLET_CORVETTE_1963)        == CarVendorGM);
    CHECK(SrLibQueryGetCarVendor(CHEVROLET_IMPALA_1958)          == CarVendorGM);
    CHECK(SrLibQueryGetCarVendor(OLDSMOBILE_88_CUSTOM_1952)      == CarVendorGM);
    CHECK(SrLibQueryGetCarVendor(PONTIAC_SILVER_STREAK_1950)     == CarVendorGM);
}

TEST_CASE("Ford cars are Ford") {
    CHECK(SrLibQueryGetCarVendor(FORD_DEUCE_COUPE_1932)          == CarVendorFord);
    CHECK(SrLibQueryGetCarVendor(FORD_THUNDERBIRD_1957)          == CarVendorFord);
    CHECK(SrLibQueryGetCarVendor(MERCURY_MONTEREY_1954)          == CarVendorFord);
}

TEST_CASE("Chrysler/Dodge/Plymouth/Valiant are Chrysler") {
    CHECK(SrLibQueryGetCarVendor(DODGE_CUSTOM_ROYAL_LANCER_1955) == CarVendorChrysler);
    CHECK(SrLibQueryGetCarVendor(DODGE_POLARA_1962)              == CarVendorChrysler);
    CHECK(SrLibQueryGetCarVendor(PLYMOUTH_SAVOY_2DR_1962)        == CarVendorChrysler);
    CHECK(SrLibQueryGetCarVendor(VALIANT_V200_1961)              == CarVendorChrysler);
}

TEST_CASE("unknown car index returns CarVendorUnknown") {
    CHECK(SrLibQueryGetCarVendor(9999) == CarVendorUnknown);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// DataParser::GetCarIndex
// ---------------------------------------------------------------------------

TEST_SUITE("DataParser::GetCarIndex") {

TEST_CASE("first car returns index 0") {
    DataParser dp;
    CHECK(dp.GetCarIndex(CHEVROLET_2DR_COUPE_1940_BODY) == 0);
}

TEST_CASE("second car returns index 1") {
    DataParser dp;
    CHECK(dp.GetCarIndex(CHEVROLET_ROADSTER_1940_BODY) == 1);
}

TEST_CASE("unmapped asset returns -1") {
    DataParser dp;
    CHECK(dp.GetCarIndex(GARAGE_SCREEN) == -1);
    CHECK(dp.GetCarIndex(DINER_SCREEN)  == -1);
}

TEST_CASE("consecutive car models produce consecutive indices") {
    DataParser dp;
    int idx1 = dp.GetCarIndex(CHEVROLET_2DR_COUPE_1940_BODY);
    int idx2 = dp.GetCarIndex(CHEVROLET_ROADSTER_1940_BODY);
    int idx3 = dp.GetCarIndex(CHEVROLET_MASTER_DELUXE_2DR_1938_BODY);
    CHECK(idx2 == idx1 + 1);
    CHECK(idx3 == idx2 + 1);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetSpriteAnimationInfo / GetAnimationFrameRect
// ---------------------------------------------------------------------------

TEST_SUITE("Sprite animation helpers") {

TEST_CASE("GetSpriteAnimationInfo returns info for known sprite") {
    const SpriteAnimationInfo* info = GetSpriteAnimationInfo(ENGINE_BELT_ANIMATION);
    REQUIRE(info != nullptr);
    CHECK(info->spriteId    == ENGINE_BELT_ANIMATION);
    CHECK(info->frameCount  == 4);
    CHECK(info->isLooping   == true);
}

TEST_CASE("GetSpriteAnimationInfo returns nullptr for unknown sprite") {
    CHECK(GetSpriteAnimationInfo(GARAGE_SCREEN) == nullptr);
    CHECK(GetSpriteAnimationInfo(9999)          == nullptr);
}

TEST_CASE("GetAnimationFrameRect returns correct frame") {
    const SpriteAnimationInfo* info = GetSpriteAnimationInfo(ENGINE_BELT_ANIMATION);
    REQUIRE(info != nullptr);
    SpriteRect r = GetAnimationFrameRect(info, 0);
    CHECK(r.x == 0);
    CHECK(r.y == 0);
    CHECK(r.w == 86);
    CHECK(r.h == 53);
}

// The original picks a frame with srcX = (counter & 3) * w, srcY = row ? h : 0
// (FUN_002416bc), off the blit record's own width and height - so frames are
// packed at exactly their own size and any slack sits at the sheet's right
// edge. Spacing them by the sheet divided by its grid instead drags in a
// strip of the next frame.
TEST_CASE("engine animation frames are packed at their own size") {
    struct Case { uint16_t id; int sheetW, sheetH, cols, rows; };
    const Case cases[] = {
        {ENGINE_BELT_ANIMATION,          176, 106, 2, 2},
        {ENGINE_PISTON_LEFT_ANIMATION,   168, 110, 4, 2},
        {ENGINE_PISTON_RIGHT_ANIMATION,  168, 108, 4, 2},
        {ENGINE_CRANK_ANIMATION,         144,  66, 2, 2},
        {ENGINE_FLYWHEEL_ANIMATION,       88,  68, 2, 2},
        {ENGINE_CAM_ANIMATION,           160,  28, 4, 2},
    };
    for (const Case& c : cases) {
        const SpriteAnimationInfo* info = GetSpriteAnimationInfo(c.id);
        REQUIRE(info != nullptr);
        CHECK(info->frameCount == c.cols * c.rows);
        const SpriteRect first = GetAnimationFrameRect(info, 0);
        for (int i = 0; i < info->frameCount; i++) {
            SpriteRect r = GetAnimationFrameRect(info, i);
            CHECK(r.w == first.w);
            CHECK(r.h == first.h);
            CHECK(r.x == (i % c.cols) * first.w);
            CHECK(r.y == (i / c.cols) * first.h);
            CHECK(r.x + r.w <= c.sheetW);
            CHECK(r.y + r.h <= c.sheetH);
        }
    }
}

TEST_CASE("GetAnimationFrameRect out-of-range returns zero rect") {
    const SpriteAnimationInfo* info = GetSpriteAnimationInfo(ENGINE_BELT_ANIMATION);
    REQUIRE(info != nullptr);
    SpriteRect r = GetAnimationFrameRect(info, 99);
    CHECK(r.w == 0);
    CHECK(r.h == 0);
}

TEST_CASE("GetAnimationFrameRect with null info returns zero rect") {
    SpriteRect r = GetAnimationFrameRect(nullptr, 0);
    CHECK(r.w == 0);
    CHECK(r.h == 0);
}

TEST_CASE("all known animated sprites have valid frame data") {
    for (uint16_t id : {(uint16_t)ENGINE_BELT_ANIMATION,
                        (uint16_t)ENGINE_PISTON_LEFT_ANIMATION,
                        (uint16_t)ENGINE_PISTON_RIGHT_ANIMATION,
                        (uint16_t)ENGINE_CRANK_ANIMATION,
                        (uint16_t)ENGINE_FLYWHEEL_ANIMATION,
                        (uint16_t)ENGINE_CAM_ANIMATION,
                        (uint16_t)CAR_JACK_ANIMATION}) {
        const SpriteAnimationInfo* info = GetSpriteAnimationInfo(id);
        REQUIRE(info != nullptr);
        CHECK(info->frameCount > 0);
        // Every frame should have non-zero dimensions
        for (int f = 0; f < info->frameCount; f++) {
            SpriteRect r = GetAnimationFrameRect(info, f);
            CHECK(r.w > 0);
            CHECK(r.h > 0);
        }
    }
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// CalculateSampleDuration
// ---------------------------------------------------------------------------

TEST_SUITE("CalculateSampleDuration") {

TEST_CASE("11025 samples at 11025 Hz = 1.0 second") {
    float d = CalculateSampleDuration(11025, 11025);
    CHECK(d == doctest::Approx(1.0f));
}

TEST_CASE("0 samples = 0 seconds") {
    CHECK(CalculateSampleDuration(0, 11025) == doctest::Approx(0.0f));
}

TEST_CASE("known engine start duration (~0.710 seconds)") {
    float d = CalculateSampleDuration(SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    CHECK(d == doctest::Approx(0.71f).epsilon(0.01f));
}

} // TEST_SUITE
