#include "doctest/doctest.h"
#include "part.h"

// SellSparePartsLoop (Amiga 0x0024284a); see docs/gameplay.md "Selling".
TEST_CASE("PartSalePrice: engines and gearboxes are priced on wear") {
    // 90% of value at zero wear, then rounded down to a multiple of 5.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 0, 0) == 450);
    CHECK(PartSalePrice(PartCategory::Transmission, 500, 0, 19) == 450);
    // Half worn, half the money.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 50, 0) == 225);
    // Worn out is worthless.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 100, 0) == 0);
    // Rounding is down, never up: 100*100*9/1000 = 90.
    CHECK(PartSalePrice(PartCategory::Engine, 103, 0, 0) == 90);
}

TEST_CASE("PartSalePrice: carbs and manifolds are priced on the roll, not wear") {
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 0, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 90, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Manifold, 100, 0, 19) == 85);
}

TEST_CASE("PartSalePrice: tires always fetch nothing") {
    CHECK(PartSalePrice(PartCategory::Tire, 150, 0, 19) == 0);
}

// Race wear: the sim hands back raw 0..10000 units, Part::wear is percent.
TEST_CASE("ApplyWearRaw: carries the sub-percent remainder between races") {
    int wear = 0, rem = 0;
    // A drag race's worth. On its own it is 1%, but the leftover 65 is kept.
    ApplyWearRaw(wear, rem, 165);
    CHECK(wear == 1);
    CHECK(rem == 65);
    // Three more of the same reach 6%, not the 4% plain truncation would give.
    ApplyWearRaw(wear, rem, 165);
    ApplyWearRaw(wear, rem, 165);
    ApplyWearRaw(wear, rem, 165);
    CHECK(wear == 6);
    CHECK(rem == 60);
}

TEST_CASE("ApplyWearRaw: a race that wore nothing changes nothing") {
    int wear = 42, rem = 7;
    ApplyWearRaw(wear, rem, 0);
    ApplyWearRaw(wear, rem, -5);
    CHECK(wear == 42);
    CHECK(rem == 7);
}

TEST_CASE("ApplyWearRaw: clamps at worn out") {
    int wear = 95, rem = 0;
    ApplyWearRaw(wear, rem, 10000);
    CHECK(wear == 100);
}

// The master parts table the whole economy is priced from (Amiga A4-0x48ec,
// 44 records of 14 bytes, price at +0). Read straight out of the binary, so
// these are the values, not a balance choice - a wrong one moves every car's
// worth, every scrap payout and every part sale with it.
TEST_CASE("PartCatalogPrice: the original's master parts table") {
    // 9 engines: three V6s at a flat $100, then small then big V8s per brand.
    CHECK(PartCatalogPrice(CarEngineGM_V6) == 100);
    CHECK(PartCatalogPrice(CarEngineFord_V6) == 100);
    CHECK(PartCatalogPrice(CarEngineChrysler_V6) == 100);
    CHECK(PartCatalogPrice(CarEngineGM_V8_282) == 350);
    CHECK(PartCatalogPrice(CarEngineFord_V8_292) == 340);
    CHECK(PartCatalogPrice(CarEngineChrysler_V8_285) == 360);
    CHECK(PartCatalogPrice(CarEngineGM_V8_327) == 500);
    CHECK(PartCatalogPrice(CarEngineFord_V8_351) == 475);
    CHECK(PartCatalogPrice(CarEngineChrysler_V8_340) == 525);

    // 13 transmissions, in the catalog's own order.
    CHECK(PartCatalogPrice(TRANS_AUTO_GM) == 130);
    CHECK(PartCatalogPrice(TRANS_AUTO_FORD) == 120);
    CHECK(PartCatalogPrice(TRANS_AUTO_CHRYSLER) == 125);
    CHECK(PartCatalogPrice(TRANS_3SPD_GM) == 175);
    CHECK(PartCatalogPrice(TRANS_3SPD_FORD) == 160);
    CHECK(PartCatalogPrice(TRANS_3SPD_CHRYSLER) == 170);
    CHECK(PartCatalogPrice(TRANS_4SPD_GM) == 250);
    CHECK(PartCatalogPrice(TRANS_4SPD_FORD) == 260);
    CHECK(PartCatalogPrice(TRANS_4SPD_CHRYSLER) == 245);
    CHECK(PartCatalogPrice(TRANS_4SPD_UNIVERSAL) == 275);
    CHECK(PartCatalogPrice(TRANS_RACING_GM) == 400);
    CHECK(PartCatalogPrice(TRANS_RACING_FORD) == 405);
    CHECK(PartCatalogPrice(TRANS_RACING_CHRYSLER) == 410);

    CHECK(PartCatalogPrice(CARB_2BARREL) == 25);
    CHECK(PartCatalogPrice(CARB_4BARREL) == 40);
    CHECK(PartCatalogPrice(CARB_4BARREL_RACING) == 60);

    // Manifolds go by class, not brand - and the two middle classes tie, so
    // this has to follow ManifoldClass's order rather than ManifoldType's.
    CHECK(PartCatalogPrice(MANIFOLD_2BARREL) == 20);
    CHECK(PartCatalogPrice(MANIFOLD_4BARREL) == 25);
    CHECK(PartCatalogPrice(MANIFOLD_TRIPLE_2BARREL) == 35);
    CHECK(PartCatalogPrice(MANIFOLD_DUAL_QUAD) == 35);
    CHECK(PartCatalogPrice(MANIFOLD_RACING) == 60);

    CHECK(PartCatalogPrice(STOCK) == 50);
    CHECK(PartCatalogPrice(BRAND) == 80);
    CHECK(PartCatalogPrice(RACING) == 150);
}
