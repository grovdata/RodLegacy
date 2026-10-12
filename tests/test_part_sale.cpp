#include "doctest/doctest.h"
#include "car/part.h"

TEST_CASE("PartSalePrice: engines and gearboxes are priced on wear") {
    // 90% of value at zero wear, then rounded down to a multiple of 5.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 0, 0) == 450);
    CHECK(PartSalePrice(PartCategory::Transmission, 500, 0, 19) == 450);
    // Half worn, half the money.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 5000, 0) == 225);
    // Worn out is worthless.
    CHECK(PartSalePrice(PartCategory::Engine, 500, WEAR_MAX, 0) == 0);
    // Rounding is down, never up: 100*100*9/1000 = 90.
    CHECK(PartSalePrice(PartCategory::Engine, 103, 0, 0) == 90);
}

TEST_CASE("PartSalePrice: carbs and manifolds are priced on the roll, not wear") {
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 0, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 9000, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Manifold, 100, 0, 19) == 85);
}

TEST_CASE("PartSalePrice: tires always fetch nothing") {
    CHECK(PartSalePrice(PartCategory::Tire, 150, 0, 19) == 0);
}

// Part::wear is the original's 0..10000 unit, so a race's worth goes on as-is.
TEST_CASE("ApplyWearRaw: adds a race's worth straight on") {
    int wear = 0;
    for (int i = 0; i < 4; ++i) ApplyWearRaw(wear, 165);
    CHECK(wear == 660);
    CHECK(WearPercent(wear) == 6);
}

TEST_CASE("ApplyWearRaw: a race that wore nothing changes nothing") {
    int wear = 4207;
    ApplyWearRaw(wear, 0);
    ApplyWearRaw(wear, -5);
    CHECK(wear == 4207);
}

TEST_CASE("ApplyWearRaw: clamps at worn out") {
    int wear = 9500;
    ApplyWearRaw(wear, 10000);
    CHECK(wear == WEAR_MAX);
    CHECK(WearPercent(wear) == 100);
}

TEST_CASE("WearPercent: what the screen shows") {
    CHECK(WearPercent(0) == 0);
    CHECK(WearPercent(99) == 0);      // under 1% still reads as new
    CHECK(WearPercent(4850) == 48);
    CHECK(WearPercent(WEAR_MAX) == 100);
    CHECK(WearPercent(-1) == 0);      // clamped, never negative
}

// The master parts table, read from the binary; a wrong value moves the whole economy.
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

TEST_CASE("SR2 under-car parts rebuilt from class carry the catalogue price") {
    CHECK(MakeSr2Part(PartCategory::ExhaustManifold, SR_EXHAUST_STOCK, CarVendorGM).cost == 15);
    CHECK(MakeSr2Part(PartCategory::ExhaustManifold, SR_EXHAUST_STOCK, CarVendorFord).cost == 10);
    CHECK(MakeSr2Part(PartCategory::ExhaustManifold, SR_EXHAUST_HEADERS, CarVendorChrysler).cost == 65);
    CHECK(MakeSr2Part(PartCategory::ExhaustManifold, SR_EXHAUST_HEADERS, CarVendorFord).vendor == CarVendorFord);
    CHECK(MakeSr2Part(PartCategory::Muffler, SR_MUFFLER_GLASSPACK, CarVendorFord).cost == 15);
    CHECK(MakeSr2Part(PartCategory::Muffler, SR_MUFFLER_STOCK, CarVendorFord).vendor == CarVendorUnknown);
    CHECK(MakeSr2Part(PartCategory::Differential, SR_DIFF_2_78, CarVendorGM).cost == 50);
    CHECK(MakeSr2Part(PartCategory::Differential, SR_DIFF_4_11, CarVendorGM).cost == 150);
    const Part headers = MakeSr2Part(PartCategory::ExhaustManifold, SR_EXHAUST_HEADERS, CarVendorGM);
    CHECK(Sr2PartName(PartCategory::ExhaustManifold, headers) == "GM tube headers");
    const Part diff = MakeSr2Part(PartCategory::Differential, SR_DIFF_3_73, CarVendorGM);
    CHECK(Sr2PartName(PartCategory::Differential, diff) == "Differential 3.73:1");
}
