#pragma once

#include "types.h"
#include "engine_part.h"
#include "car.h"
#include "cardata.h"
#include "car_system.h"

class CarBuilder {
public:
    // Build a car with proper engine matching the vendor
    static CarPtr BuildCar(CarModel model);

    // Build a car with random parts (for roster generation)
    static CarPtr BuildRandomCar(CarModel model);

    // Public so save-loading code can build a Part with the same asset-ID
    // mapping used everywhere else, without duplicating these tables.
    static Part CreateTransmissionPart(TransmissionType type, CarVendor_t vendor);
    static Part CreateManifoldPart(ManifoldType type, CarVendor_t vendor);
    static Part CreateCarburetorPart(CarburetorType type, CarVendor_t vendor);

    // Decode a save file's part "variant" byte (save_game.md) into a real
    // type. Returns false (leaving out untouched) when the variant isn't
    // one this has a confirmed mapping for, so callers can fall back
    // explicitly instead of guessing.
    static bool TransmissionTypeFromSaveVariant(int variant, TransmissionType& out);
    static bool CarburetorTypeFromSaveVariant(int variant, CarburetorType& out);
    static bool EngineTypeFromSaveVariant(int variant, CarEngine_t& outType, AmigaLibAsset& outAsset);
    static bool ManifoldTypeFromSaveVariant(int variant, ManifoldType& out);
    static bool TireTypeFromSaveVariant(int variant, CarWheelType& out);

    // Overwrite a car's drivetrain with the parts its SR_data spec says it
    // carries, and hand it the spec's class byte for the top-speed weight
    // term. A car for sale in the newspaper ships with fixed parts in the
    // original, not random ones.
    static void ApplySpec(Car* car, const SrCarSpecEntry& spec, CarVendor_t vendor);

private:
    // Helper to get appropriate engine type for vendor
    static CarEngine_t GetDefaultEngineForVendor(CarVendor_t vendor);

    // Helper to get random engine type for vendor

    // New helper methods for compatibility-based selection
    static CarEngine_t SelectCompatibleEngine(const CarModel& model);
    static TransmissionType SelectCompatibleTransmission(CarEngine_t engine);
    static ManifoldType SelectCompatibleManifold(CarEngine_t engine);
    static CarburetorType SelectCompatibleCarburetor(ManifoldType manifold);
};
