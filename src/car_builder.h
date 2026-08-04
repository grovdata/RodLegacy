#pragma once

#include "types.h"
#include "engine_part.h"
#include "car.h"
#include "cardata.h"
#include "car_system.h"

class CarBuilder {
public:
    // Build a car with proper engine matching the vendor
    static Car* BuildCar(CarModel model);

    // Build a car with random parts (for roster generation)
    static Car* BuildRandomCar(CarModel model);

    // Public so save-loading code can build a Part with the same asset-ID
    // mapping used everywhere else, without duplicating these tables.
    static Part CreateTransmissionPart(TransmissionType type, CarVendor_t vendor);
    static Part CreateManifoldPart(ManifoldType type, CarVendor_t vendor);
    static Part CreateCarburetorPart(CarburetorType type, CarVendor_t vendor);

private:
    // Helper to get appropriate engine type for vendor
    static CarEngine_t GetDefaultEngineForVendor(CarVendor_t vendor);

    // Helper to get random engine type for vendor

    // New helper methods for compatibility-based selection
    static CarEngine_t SelectCompatibleEngine(const CarModel& model);
    static TransmissionType SelectCompatibleTransmission(CarEngine_t engine);
    static ManifoldType SelectCompatibleManifold(CarEngine_t engine);
    static CarburetorType SelectCompatibleCarburetor(CarEngine_t engine);
};
