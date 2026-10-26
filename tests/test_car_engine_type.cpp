#include "doctest/doctest.h"
#include "car/engine_part.h"

// A car still on its SR_data spec has no concrete EnginePart, so the type resolves from the spec class.
TEST_CASE("spec engine class resolves to the car's own brand and tier") {
    CHECK(SpecEngineType(CarVendorGM, 0) == CarEngineGM_V6);
    CHECK(SpecEngineType(CarVendorGM, 1) == CarEngineGM_V8_282);
    CHECK(SpecEngineType(CarVendorGM, 2) == CarEngineGM_V8_327);

    CHECK(SpecEngineType(CarVendorFord, 0) == CarEngineFord_V6);
    CHECK(SpecEngineType(CarVendorFord, 2) == CarEngineFord_V8_351);

    CHECK(SpecEngineType(CarVendorChrysler, 1) == CarEngineChrysler_V8_285);

    // Custom/unknown brands fall back to the GM row rather than running off
    // the end of the enum.
    CHECK(SpecEngineType(CarVendorCustom, 2) == CarEngineGM_V8_327);
    CHECK(SpecEngineType(CarVendorGeneric, 0) == CarEngineGM_V6);
}

TEST_CASE("out-of-range spec classes clamp into the brand's own row") {
    // ApplySpec clamps EngineClass to 0..3, so a 3 does reach here.
    CHECK(SpecEngineType(CarVendorFord, 3) == CarEngineFord_V8_351);
    CHECK(SpecEngineType(CarVendorFord, -1) == CarEngineFord_V6);
}

// Only class 2 gets a scoop: the 1957 Fairlane 500 2-dr is class 1 and has none in the original.
TEST_CASE("the scoop follows the tier, in every brand") {
    for (CarVendor_t v : {CarVendorGM, CarVendorFord, CarVendorChrysler}) {
        CHECK(EngineTypeHasScoop(SpecEngineType(v, 0)) == false);
        CHECK(EngineTypeHasScoop(SpecEngineType(v, 1)) == false);
        CHECK(EngineTypeHasScoop(SpecEngineType(v, 2)) == true);
    }
}

TEST_CASE("the resolved tier matches what the engine bay draws") {
    // EngineBodyAsset picks the body off the tier, so tier and displacement
    // have to agree for every brand - a V6 class must never yield a V8.
    for (CarVendor_t v : {CarVendorGM, CarVendorFord, CarVendorChrysler}) {
        int previous = 0;
        for (int cls = 0; cls <= 2; cls++) {
            const CarEngine_t type = SpecEngineType(v, cls);
            CHECK(static_cast<int>(type) % 3 == cls);
            const EnginePart part(type, 0, 0, v, ENGINE_V8);
            if (cls == 0) CHECK(part.displacement == 200);
            CHECK(part.displacement > previous);
            previous = part.displacement;
        }
    }
}
