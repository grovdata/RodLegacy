#include "car_builder.h"
#include "car.h"
#include "parsers/libp.h"
#include "car_system.h"
#include <random>
#include <algorithm>

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
    
    return car;
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

CarEngine_t CarBuilder::GetRandomEngineForVendor(CarVendor_t vendor) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    std::vector<CarEngine_t> vendorEngines;
    
    switch (vendor) {
        case CarVendorGM:
            vendorEngines = {CarEngineGM_V6, CarEngineGM_V8_282, CarEngineGM_V8_327};
            break;
        case CarVendorFord:
            vendorEngines = {CarEngineFord_V6, CarEngineFord_V8_292, CarEngineFord_V8_351};
            break;
        case CarVendorChrysler:
            vendorEngines = {CarEngineChrysler_V6, CarEngineChrysler_V8_285, CarEngineChrysler_V8_340};
            break;
        default:
            vendorEngines = {CarEngineGM_V8_327}; // Default fallback
            break;
    }
    
    std::uniform_int_distribution<> dist(0, vendorEngines.size() - 1);
    return vendorEngines[dist(gen)];
}

CarEngine_t CarBuilder::SelectCompatibleEngine(const CarModel& model) {
    if (model.compatibleEngines.empty()) {
        return GetDefaultEngineForVendor(model.vendor);
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, model.compatibleEngines.size() - 1);
    return model.compatibleEngines[dist(gen)];
}

TransmissionType CarBuilder::SelectCompatibleTransmission(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleTransmissions(engine);
    if (compatible.empty()) {
        return TRANS_4SPD_UNIVERSAL; // Default fallback
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(gen)];
}

ManifoldType CarBuilder::SelectCompatibleManifold(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleManifolds(engine);
    if (compatible.empty()) {
        return MANIFOLD_STOCK; // Default fallback
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(gen)];
}

CarburetorType CarBuilder::SelectCompatibleCarburetor(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleCarburetors(engine);
    if (compatible.empty()) {
        return CARB_2BARREL; // Default fallback
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(gen)];
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
        case MANIFOLD_STOCK: assetId = ENGINE_MANIFOLD1; break;
        case MANIFOLD_PERFORMANCE: assetId = ENGINE_MANIFOLD3; break;
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
