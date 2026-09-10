#include <doctest/doctest.h>
#include "engine_part.h"

// ---------------------------------------------------------------------------
// Default constructor
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart default constructor") {

TEST_CASE("default engine is GM V8 327 with correct stats") {
    EnginePart ep;
    CHECK(ep.engineType   == CarEngineGM_V8_327);
    CHECK(ep.horsepower   == 300);
    CHECK(ep.torque       == 360);
    CHECK(ep.displacement == 327);
    CHECK(ep.wear  == 0);
    CHECK(ep.cost  == 0);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// Performance characteristics for all 9 engine types
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart performance characteristics") {

TEST_CASE("GM V6: 180 HP / 220 torque / 200 ci") {
    EnginePart ep(CarEngineGM_V6, 0, 0, CarVendorGM, ENGINE_V8);
    CHECK(ep.horsepower   == 180);
    CHECK(ep.torque       == 220);
    CHECK(ep.displacement == 200);
}

TEST_CASE("GM V8 282: 250 HP / 300 torque / 282 ci") {
    EnginePart ep(CarEngineGM_V8_282, 0, 0, CarVendorGM, ENGINE_V8);
    CHECK(ep.horsepower   == 250);
    CHECK(ep.torque       == 300);
    CHECK(ep.displacement == 282);
}

TEST_CASE("GM V8 327: 300 HP / 360 torque / 327 ci") {
    EnginePart ep(CarEngineGM_V8_327, 0, 0, CarVendorGM, ENGINE_V8);
    CHECK(ep.horsepower   == 300);
    CHECK(ep.torque       == 360);
    CHECK(ep.displacement == 327);
}

TEST_CASE("Ford V6: 190 HP / 230 torque / 200 ci") {
    EnginePart ep(CarEngineFord_V6, 0, 0, CarVendorFord, ENGINE_V8);
    CHECK(ep.horsepower   == 190);
    CHECK(ep.torque       == 230);
    CHECK(ep.displacement == 200);
}

TEST_CASE("Ford V8 292: 260 HP / 310 torque / 292 ci") {
    EnginePart ep(CarEngineFord_V8_292, 0, 0, CarVendorFord, ENGINE_V8);
    CHECK(ep.horsepower   == 260);
    CHECK(ep.torque       == 310);
    CHECK(ep.displacement == 292);
}

TEST_CASE("Ford V8 351: 320 HP / 380 torque / 351 ci") {
    EnginePart ep(CarEngineFord_V8_351, 0, 0, CarVendorFord, ENGINE_V8);
    CHECK(ep.horsepower   == 320);
    CHECK(ep.torque       == 380);
    CHECK(ep.displacement == 351);
}

TEST_CASE("Chrysler V6: 185 HP / 225 torque / 200 ci") {
    EnginePart ep(CarEngineChrysler_V6, 0, 0, CarVendorChrysler, ENGINE_V8);
    CHECK(ep.horsepower   == 185);
    CHECK(ep.torque       == 225);
    CHECK(ep.displacement == 200);
}

TEST_CASE("Chrysler V8 285: 255 HP / 305 torque / 285 ci") {
    EnginePart ep(CarEngineChrysler_V8_285, 0, 0, CarVendorChrysler, ENGINE_V8);
    CHECK(ep.horsepower   == 255);
    CHECK(ep.torque       == 305);
    CHECK(ep.displacement == 285);
}

TEST_CASE("Chrysler V8 340: 290 HP / 340 torque / 340 ci") {
    EnginePart ep(CarEngineChrysler_V8_340, 0, 0, CarVendorChrysler, ENGINE_V8);
    CHECK(ep.horsepower   == 290);
    CHECK(ep.torque       == 340);
    CHECK(ep.displacement == 340);
}

TEST_CASE("wear and cost are preserved by constructor") {
    EnginePart ep(CarEngineGM_V8_327, 42, 1500, CarVendorGM, ENGINE_V8);
    CHECK(ep.wear == 42);
    CHECK(ep.cost == 1500);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// HasScoop: true only for the class-2 (big V8) engines
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart::HasScoop") {

TEST_CASE("only the 3 class-2 engines have a scoop") {
    CHECK(EnginePart(CarEngineGM_V8_327,       0, 0, CarVendorGM,       ENGINE_V8).HasScoop() == true);
    CHECK(EnginePart(CarEngineFord_V8_351,     0, 0, CarVendorFord,     ENGINE_V8).HasScoop() == true);
    CHECK(EnginePart(CarEngineChrysler_V8_340, 0, 0, CarVendorChrysler, ENGINE_V8).HasScoop() == true);
}

TEST_CASE("the small V8s have no scoop") {
    CHECK(EnginePart(CarEngineGM_V8_282,       0, 0, CarVendorGM,       ENGINE_V8).HasScoop() == false);
    CHECK(EnginePart(CarEngineFord_V8_292,     0, 0, CarVendorFord,     ENGINE_V8).HasScoop() == false);
    CHECK(EnginePart(CarEngineChrysler_V8_285, 0, 0, CarVendorChrysler, ENGINE_V8).HasScoop() == false);
}

TEST_CASE("all 3 V6 engines have no scoop") {
    CHECK(EnginePart(CarEngineGM_V6,       0, 0, CarVendorGM,       ENGINE_V8).HasScoop() == false);
    CHECK(EnginePart(CarEngineFord_V6,     0, 0, CarVendorFord,     ENGINE_V8).HasScoop() == false);
    CHECK(EnginePart(CarEngineChrysler_V6, 0, 0, CarVendorChrysler, ENGINE_V8).HasScoop() == false);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetVendor
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart::GetVendor") {

TEST_CASE("GM engine types return CarVendorGM") {
    CHECK(EnginePart(CarEngineGM_V6,    0, 0, CarVendorGM, ENGINE_V8).GetVendor() == CarVendorGM);
    CHECK(EnginePart(CarEngineGM_V8_282, 0, 0, CarVendorGM, ENGINE_V8).GetVendor() == CarVendorGM);
    CHECK(EnginePart(CarEngineGM_V8_327, 0, 0, CarVendorGM, ENGINE_V8).GetVendor() == CarVendorGM);
}

TEST_CASE("Ford engine types return CarVendorFord") {
    CHECK(EnginePart(CarEngineFord_V6,    0, 0, CarVendorFord, ENGINE_V8).GetVendor() == CarVendorFord);
    CHECK(EnginePart(CarEngineFord_V8_292, 0, 0, CarVendorFord, ENGINE_V8).GetVendor() == CarVendorFord);
    CHECK(EnginePart(CarEngineFord_V8_351, 0, 0, CarVendorFord, ENGINE_V8).GetVendor() == CarVendorFord);
}

TEST_CASE("Chrysler engine types return CarVendorChrysler") {
    CHECK(EnginePart(CarEngineChrysler_V6,    0, 0, CarVendorChrysler, ENGINE_V8).GetVendor() == CarVendorChrysler);
    CHECK(EnginePart(CarEngineChrysler_V8_285, 0, 0, CarVendorChrysler, ENGINE_V8).GetVendor() == CarVendorChrysler);
    CHECK(EnginePart(CarEngineChrysler_V8_340, 0, 0, CarVendorChrysler, ENGINE_V8).GetVendor() == CarVendorChrysler);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// GetEngineName
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart::GetEngineName") {

TEST_CASE("all 9 engine types return correct name string") {
    CHECK(EnginePart(CarEngineGM_V6,           0, 0, CarVendorGM,       ENGINE_V8).GetEngineName() == "GM V6");
    CHECK(EnginePart(CarEngineGM_V8_282,       0, 0, CarVendorGM,       ENGINE_V8).GetEngineName() == "GM V8 282");
    CHECK(EnginePart(CarEngineGM_V8_327,       0, 0, CarVendorGM,       ENGINE_V8).GetEngineName() == "GM V8 327");
    CHECK(EnginePart(CarEngineFord_V6,         0, 0, CarVendorFord,     ENGINE_V8).GetEngineName() == "Ford V6");
    CHECK(EnginePart(CarEngineFord_V8_292,     0, 0, CarVendorFord,     ENGINE_V8).GetEngineName() == "Ford V8 292");
    CHECK(EnginePart(CarEngineFord_V8_351,     0, 0, CarVendorFord,     ENGINE_V8).GetEngineName() == "Ford V8 351");
    CHECK(EnginePart(CarEngineChrysler_V6,     0, 0, CarVendorChrysler, ENGINE_V8).GetEngineName() == "Chrysler V6");
    CHECK(EnginePart(CarEngineChrysler_V8_285, 0, 0, CarVendorChrysler, ENGINE_V8).GetEngineName() == "Chrysler V8 285");
    CHECK(EnginePart(CarEngineChrysler_V8_340, 0, 0, CarVendorChrysler, ENGINE_V8).GetEngineName() == "Chrysler V8 340");
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// FromPart: vendor→default engine mapping
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart::FromPart") {

TEST_CASE("GM vendor maps to GM V8 327") {
    Part p(10, 500, CarVendorGM, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
    EnginePart ep = EnginePart::FromPart(p);
    CHECK(ep.engineType == CarEngineGM_V8_327);
    CHECK(ep.wear  == 10);
    CHECK(ep.cost  == 500);
    CHECK(ep.vendor == CarVendorGM);
}

TEST_CASE("Ford vendor maps to Ford V8 351 with correct HP") {
    Part p(5, 700, CarVendorFord, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
    EnginePart ep = EnginePart::FromPart(p);
    CHECK(ep.engineType == CarEngineFord_V8_351);
    CHECK(ep.horsepower == 320);
}

TEST_CASE("Chrysler vendor maps to Chrysler V8 340 with correct HP") {
    Part p(0, 800, CarVendorChrysler, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
    EnginePart ep = EnginePart::FromPart(p);
    CHECK(ep.engineType == CarEngineChrysler_V8_340);
    CHECK(ep.horsepower == 290);
}

TEST_CASE("unknown vendor falls back to GM V8 327") {
    Part p(0, 0, CarVendorUnknown, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
    EnginePart ep = EnginePart::FromPart(p);
    CHECK(ep.engineType == CarEngineGM_V8_327);
}

} // TEST_SUITE

// ---------------------------------------------------------------------------
// ToPart: round-trip preservation of wear, cost, vendor, assetId
// ---------------------------------------------------------------------------

TEST_SUITE("EnginePart::ToPart") {

TEST_CASE("ToPart preserves wear, cost, vendor and assetId") {
    EnginePart ep(CarEngineGM_V8_327, 15, 1200, CarVendorGM, ENGINE_V8);
    Part p = ep.ToPart();
    CHECK(p.wear     == 15);
    CHECK(p.cost     == 1200);
    CHECK(p.vendor   == CarVendorGM);
    CHECK(p.assetId  == ENGINE_V8);
}

} // TEST_SUITE
