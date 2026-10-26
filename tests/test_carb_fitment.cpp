#include <doctest/doctest.h>
#include "car/car_builder.h"
#include "car/engine_bay.h"
#include <SDL_log.h>

// Link stubs: the real ones need SDL and the engine bay renderer, and the car model never calls them here.
AmigaLibAsset ManifoldBodyAsset(ManifoldType, CarEngine_t) { return ENGINE_V8; }
extern "C" void SDL_LogInfo(int, const char*, ...) {}

TEST_SUITE("Carburetor fitment") {

TEST_CASE("a carb built for a GM car fits a Ford car") {
    const Part carb = CarBuilder::CreateCarburetorPart(CARB_4BARREL_RACING);
    CHECK(PartsCompatibilityManager::IsVendorCompatible(carb.vendor, CarVendorFord));
    CHECK(PartsCompatibilityManager::IsCarburetorCompatible(carb.partType.carburetorType, MANIFOLD_RACING));
}

}

#include "car/car.h"

TEST_SUITE("Stock parts reinstall") {

// Every spec combination a car can be built from: the stock gearbox and carb must go back on the car they came off.
TEST_CASE("stock parts pass the make check on their own car") {
    const CarVendor_t makes[] = {CarVendorGM, CarVendorFord, CarVendorChrysler};
    for (CarVendor_t make : makes) {
        for (int trans = 0; trans < 4; trans++)
        for (int carb = 0; carb < 3; carb++)
        for (int tire = 0; tire < 3; tire++) {
            SrCarSpecEntry spec{};
            const int config = (tire << 10) | (trans << 8) | (carb << 6);
            spec.config[0] = config >> 8;
            spec.config[1] = config & 0xFF;
            CarModel model{};
            model.vendor = make;
            Car car(model);
            CarBuilder::ApplySpec(&car, spec, make);
            CAPTURE(make); CAPTURE(trans); CAPTURE(carb);
            CHECK(PartsCompatibilityManager::IsVendorCompatible(car.GetGearbox().vendor, make));
            CHECK(PartsCompatibilityManager::IsVendorCompatible(car.GetCarburator().vendor, make));
        }
    }
}

TEST_CASE("a carb or universal gearbox from any car fits every make") {
    const CarVendor_t makes[] = {CarVendorGM, CarVendorFord, CarVendorChrysler};
    for (CarVendor_t to : makes)
    for (int c = 0; c < 3; c++) {
        const Part carb = CarBuilder::CreateCarburetorPart(static_cast<CarburetorType>(c));
        CHECK(PartsCompatibilityManager::IsVendorCompatible(carb.vendor, to));
    }
    const Part uni = CarBuilder::CreateTransmissionPart(TRANS_4SPD_UNIVERSAL);
    for (CarVendor_t to : makes) CHECK(PartsCompatibilityManager::IsVendorCompatible(uni.vendor, to));
}

TEST_CASE("a make-specific gearbox carries its own make") {
    CHECK(CarBuilder::CreateTransmissionPart(TRANS_RACING_FORD).vendor == CarVendorFord);
    CHECK(CarBuilder::CreateTransmissionPart(TRANS_AUTO_GM).vendor == CarVendorGM);
    CHECK(CarBuilder::CreateTransmissionPart(TRANS_3SPD_CHRYSLER).vendor == CarVendorChrysler);
}

}
