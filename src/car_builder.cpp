#include "car_builder.h"
#include "car.h"
#include "parsers/libp.h"
#include "car_system.h"
#include <random>
#include <algorithm>

namespace {
std::mt19937& RandomEngine() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}
}

Car* CarBuilder::BuildCar(CarModel model) {
    Car* car = new Car(model);

    // Set up proper engine matching the vendor
    CarEngine_t engineType = GetDefaultEngineForVendor(model.vendor);
    EnginePart engine(engineType, 0, 0, model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    return car;
}

Car* CarBuilder::BuildRandomCar(CarModel model) {
    Car* car = new Car(model);

    // Select compatible engine
    CarEngine_t engineType = SelectCompatibleEngine(model);
    EnginePart engine(engineType, 0, 0, model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    // Select compatible transmission
    TransmissionType transType = SelectCompatibleTransmission(engineType);
    Part transmission = CreateTransmissionPart(transType, model.vendor);
    car->SetGearbox(transmission);

    // Select compatible manifold
    ManifoldType manifoldType = SelectCompatibleManifold(engineType);
    Part manifold = CreateManifoldPart(manifoldType, model.vendor);
    car->SetManifold(manifold);

    // Select compatible carburetor
    CarburetorType carbType = SelectCompatibleCarburetor(engineType);
    Part carburetor = CreateCarburetorPart(carbType, model.vendor);
    car->SetCarburator(carburetor);

    // Random tires
    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);
    car->SetTires((CarWheelType)tireDist(RandomEngine()));

    return car;
}

void CarBuilder::ApplySpec(Car* car, const SrCarSpecEntry& spec, CarVendor_t vendor) {
    if (!car) {
        return;
    }

    static const CarburetorType kCarbByClass[3] = {
        CARB_2BARREL, CARB_4BARREL, CARB_4BARREL_RACING
    };
    // Transmission class 0..3 = auto / 3-speed / 4-speed / racing, picked in
    // the car's own brand.
    static const TransmissionType kTransByClass[3][4] = {
        {TRANS_AUTO_GM,       TRANS_3SPD_GM,       TRANS_4SPD_GM,       TRANS_RACING_GM},
        {TRANS_AUTO_FORD,     TRANS_3SPD_FORD,     TRANS_4SPD_FORD,     TRANS_RACING_FORD},
        {TRANS_AUTO_CHRYSLER, TRANS_3SPD_CHRYSLER, TRANS_4SPD_CHRYSLER, TRANS_RACING_CHRYSLER},
    };
    static const CarWheelType kTiresByClass[3] = {STOCK, BRAND, RACING};

    int vendorRow = 0;
    switch (vendor) {
        case CarVendorFord: vendorRow = 1; break;
        case CarVendorChrysler: vendorRow = 2; break;
        default: vendorRow = 0; break;
    }

    const int carbClass = std::clamp(spec.CarbClass(), 0, 2);
    const int transClass = std::clamp(spec.TransClass(), 0, 3);
    const int tireClass = std::clamp(spec.TireClass(), 0, 2);

    car->SetCarburator(CreateCarburetorPart(kCarbByClass[carbClass], vendor));
    car->SetGearbox(CreateTransmissionPart(kTransByClass[vendorRow][transClass], vendor));
    car->SetTires(kTiresByClass[tireClass]);
    car->SetWeightClass(spec.car_class);

    // Engine and manifold go in as raw scoring classes rather than as parts:
    // the spec's manifold field runs 0..4 and RodLegacy's ManifoldType only has
    // three values, so a class-1 manifold (which half the shipped cars carry)
    // has nowhere to live. The Car keeps the class until the player fits a
    // different part. Must come last - SetManifold above would clear it.
    car->SetSpecPartClasses(std::clamp(spec.EngineClass(), 0, 3),
                            std::clamp(spec.ManifoldClass(), 0, 4));
}

CarEngine_t CarBuilder::GetDefaultEngineForVendor(CarVendor_t vendor) {
    switch (vendor) {
        case CarVendorGM:
            return CarEngineGM_V6;
        case CarVendorFord:
            return CarEngineFord_V6;
        case CarVendorChrysler:
            return CarEngineChrysler_V6;
        case CarVendorCustom:
            return CarEngineGM_V6; // Custom cars might use GM as base
        case CarVendorUnknown:
            return CarEngineGM_V6; // Unknown vendor defaults to GM
        default:
            return CarEngineGM_V6; // This should never happen
    }
}

CarEngine_t CarBuilder::SelectCompatibleEngine(const CarModel& model) {
    if (model.compatibleEngines.empty()) {
        return GetDefaultEngineForVendor(model.vendor);
    }

    std::uniform_int_distribution<> dist(0, model.compatibleEngines.size() - 1);
    return model.compatibleEngines[dist(RandomEngine())];
}

TransmissionType CarBuilder::SelectCompatibleTransmission(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleTransmissions(engine);
    if (compatible.empty()) {
        return TRANS_4SPD_UNIVERSAL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

ManifoldType CarBuilder::SelectCompatibleManifold(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleManifolds(engine);
    if (compatible.empty()) {
        return MANIFOLD_2BARREL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

CarburetorType CarBuilder::SelectCompatibleCarburetor(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleCarburetors(engine);
    if (compatible.empty()) {
        return CARB_2BARREL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

Part CarBuilder::CreateTransmissionPart(TransmissionType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case TRANS_AUTO_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_GM); break;
        case TRANS_AUTO_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_FORD); break;
        case TRANS_AUTO_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_CHRYSLER); break;
        case TRANS_3SPD_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_GM); break;
        case TRANS_3SPD_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_FORD); break;
        case TRANS_3SPD_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_CHRYSLER); break;
        case TRANS_4SPD_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_GM); break;
        case TRANS_4SPD_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_FORD); break;
        case TRANS_4SPD_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_CHRYSLER); break;
        case TRANS_RACING_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_GM); break;
        case TRANS_RACING_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_FORD); break;
        case TRANS_RACING_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_CHRYSLER); break;
        case TRANS_4SPD_UNIVERSAL: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_UNIVERSAL); break;
    }
    return Part(0, 0, vendor, assetId, type);
}

Part CarBuilder::CreateManifoldPart(ManifoldType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case MANIFOLD_2BARREL: assetId = ENGINE_MANIFOLD1; break;
        case MANIFOLD_4BARREL: assetId = ENGINE_MANIFOLD2; break;
        case MANIFOLD_DUAL_QUAD: assetId = ENGINE_MANIFOLD3; break;
        case MANIFOLD_TRIPLE_2BARREL: assetId = ENGINE_MANIFOLD4; break;
        case MANIFOLD_RACING: assetId = ENGINE_MANIFOLD6; break;
    }
    return Part(0, 0, vendor, assetId, type);
}

Part CarBuilder::CreateCarburetorPart(CarburetorType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case CARB_2BARREL: assetId = ENGINE_CARB_2BARREL; break;
        case CARB_4BARREL: assetId = ENGINE_CARB_4BARREL; break;
        case CARB_4BARREL_RACING: assetId = ENGINE_CARB_4BARREL_RACING; break;
    }
    return Part(0, 0, vendor, assetId, type);
}
