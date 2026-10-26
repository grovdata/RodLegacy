#include "car/car_builder.h"
#include "car/car.h"
#include "car/engine_bay.h"
#include "parsers/libp.h"
#include "car/car_system.h"
#include "core/utils.h"
#include <random>
#include <algorithm>

CarPtr CarBuilder::BuildCar(CarModel model) {
    CarPtr car = std::make_shared<Car>(model);

    CarEngine_t engineType = GetDefaultEngineForVendor(model.vendor);
    EnginePart engine(engineType, 0, PartCatalogPrice(engineType), model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    car->SetValue(model.price);
    return car;
}

CarPtr CarBuilder::BuildRandomCar(CarModel model) {
    CarPtr car = std::make_shared<Car>(model);

    CarEngine_t engineType = SelectCompatibleEngine(model);
    EnginePart engine(engineType, 0, PartCatalogPrice(engineType), model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    TransmissionType transType = SelectCompatibleTransmission(engineType);
    Part transmission = CreateTransmissionPart(transType);
    car->SetGearbox(transmission);

    ManifoldType manifoldType = SelectCompatibleManifold(engineType);
    Part manifold = CreateManifoldPart(manifoldType, model.vendor);
    car->SetManifold(manifold);

    // The carb must fit the manifold just chosen, not just the engine.
    CarburetorType carbType = SelectCompatibleCarburetor(manifoldType);
    Part carburetor = CreateCarburetorPart(carbType);
    car->SetCarburator(carburetor);

    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);
    car->SetTires((CarWheelType)tireDist(RandomEngine()));

    // Last: the original seeds the value with the list price and only counts parts fitted after.
    car->SetValue(model.price);
    return car;
}

void CarBuilder::ApplySpec(Car* car, const SrCarSpecEntry& spec, CarVendor_t vendor) {
    if (!car) {
        return;
    }

    static const CarburetorType kCarbByClass[3] = {
        CARB_2BARREL, CARB_4BARREL, CARB_4BARREL_RACING
    };
    // Class 0..3 = auto / 3-speed / 4-speed / racing, in the car's own brand.
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

    // Street Rod 2's three extra slots come off the spec too.
    car->SetSr2Parts(spec.exhaust, spec.muffler, spec.differential);

    car->SetCarburator(CreateCarburetorPart(kCarbByClass[carbClass]));
    car->SetGearbox(CreateTransmissionPart(kTransByClass[vendorRow][transClass]));
    car->SetTires(kTiresByClass[tireClass]);
    car->SetWeightClass(spec.car_class);

    // Raw scoring classes: ManifoldType has no value for spec class 1, which half the cars carry. Must come
    // last, as SetManifold clears it.
    car->SetSpecPartClasses(std::clamp(spec.EngineClass(), 0, 3),
                            std::clamp(spec.ManifoldClass(), 0, 4));

    car->SetValue(spec.Price());
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
        case CarVendorGeneric:
            return CarEngineGM_V6; // Unknown vendor defaults to GM
        default:
            return CarEngineGM_V6;
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
        return TRANS_4SPD_UNIVERSAL;
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

ManifoldType CarBuilder::SelectCompatibleManifold(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleManifolds(engine);
    if (compatible.empty()) {
        return MANIFOLD_2BARREL;
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

CarburetorType CarBuilder::SelectCompatibleCarburetor(ManifoldType manifold) {
    auto compatible = PartsCompatibilityManager::GetCompatibleCarburetors(manifold);
    if (compatible.empty()) {
        return CARB_2BARREL;
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

Part CarBuilder::CreateTransmissionPart(TransmissionType type) {
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
    // Catalog order is GM, Ford, Chrysler within each kind.
    static const CarVendor_t kMakes[3] = {CarVendorGM, CarVendorFord, CarVendorChrysler};
    const CarVendor_t vendor = type == TRANS_4SPD_UNIVERSAL ? CarVendorGeneric : kMakes[type % 3];
    return Part(0, PartCatalogPrice(type), vendor, assetId, type);
}

Part CarBuilder::CreateManifoldPart(ManifoldType type, CarVendor_t vendor) {
    // The V8 only picks the wide 2-barrel body for the part's stored id; drawing sites use the real engine.
    const AmigaLibAsset assetId = ManifoldBodyAsset(type, CarEngineGM_V8_327);
    return Part(0, PartCatalogPrice(type), vendor, assetId, type);
}

Part CarBuilder::CreateCarburetorPart(CarburetorType type) {
    AmigaLibAsset assetId;
    switch (type) {
        case CARB_2BARREL: assetId = ENGINE_CARB_2BARREL; break;
        case CARB_4BARREL: assetId = ENGINE_CARB_4BARREL; break;
        case CARB_4BARREL_RACING: assetId = ENGINE_CARB_4BARREL_RACING; break;
    }
    return Part(0, PartCatalogPrice(type), CarVendorGeneric, assetId, type);
}

// The save's variant byte indexes one flat master parts table. Transmissions are 9-21 in catalog order.
bool CarBuilder::TransmissionTypeFromSaveVariant(int variant, TransmissionType& out) {
    static constexpr TransmissionType kCatalogOrder[SR_TRANS_COUNT] = {
        TRANS_AUTO_GM, TRANS_AUTO_FORD, TRANS_AUTO_CHRYSLER,
        TRANS_3SPD_GM, TRANS_3SPD_FORD, TRANS_3SPD_CHRYSLER,
        TRANS_4SPD_GM, TRANS_4SPD_FORD, TRANS_4SPD_CHRYSLER, TRANS_4SPD_UNIVERSAL,
        TRANS_RACING_GM, TRANS_RACING_FORD, TRANS_RACING_CHRYSLER,
    };
    int catalogIndex = variant - 9;
    if (catalogIndex < 0 || catalogIndex >= SR_TRANS_COUNT) return false;
    out = kCatalogOrder[catalogIndex];
    return true;
}

// Carbs are 22-24 in ascending price order, all confirmed from saves.
bool CarBuilder::CarburetorTypeFromSaveVariant(int variant, CarburetorType& out) {
    switch (variant) {
        case 22: out = CARB_2BARREL; return true;
        case 23: out = CARB_4BARREL; return true;
        case 24: out = CARB_4BARREL_RACING; return true;
    }
    return false;
}

// 0=GM V6, 3=GM small V8, 6=GM big V8 are confirmed; the Ford/Chrysler entries are inferred from the
// GM, Ford, Chrysler order per tier.
bool CarBuilder::EngineTypeFromSaveVariant(int variant, CarEngine_t& outType, AmigaLibAsset& outAsset) {
    switch (variant) {
        case 0: outType = CarEngineGM_V6; outAsset = ENGINE_V6; return true;
        case 1: outType = CarEngineFord_V6; outAsset = ENGINE_V6; return true;
        case 2: outType = CarEngineChrysler_V6; outAsset = ENGINE_V6; return true;
        case 3: outType = CarEngineGM_V8_282; outAsset = ENGINE_V8; return true;
        case 4: outType = CarEngineFord_V8_292; outAsset = ENGINE_V8; return true;
        case 5: outType = CarEngineChrysler_V8_285; outAsset = ENGINE_V8; return true;
        case 6: outType = CarEngineGM_V8_327; outAsset = ENGINE_V8_BIG; return true;
        case 7: outType = CarEngineFord_V8_351; outAsset = ENGINE_V8_BIG; return true;
        case 8: outType = CarEngineChrysler_V8_340; outAsset = ENGINE_V8_BIG; return true;
    }
    return false;
}

// Manifolds start at 25. The GM column (25, 31, 34, 37) is save-verified; Ford/Chrysler are inferred from
// the stride-3 vendor order.
bool CarBuilder::ManifoldTypeFromSaveVariant(int variant, ManifoldType& out) {
    switch (variant) {
        case 25: case 26: case 27: out = MANIFOLD_2BARREL; return true;
        case 28: case 29: case 30: out = MANIFOLD_4BARREL; return true;
        case 31: case 32: case 33: out = MANIFOLD_TRIPLE_2BARREL; return true;
        case 34: case 35: case 36: out = MANIFOLD_DUAL_QUAD; return true;
        case 37: case 38: case 39: out = MANIFOLD_RACING; return true;
    }
    return false;
}

// 40 = stock, 41 = brand-name, 42 = racing, all confirmed from saves.
bool CarBuilder::TireTypeFromSaveVariant(int variant, CarWheelType& out) {
    switch (variant) {
        case 40: out = STOCK; return true;
        case 41: out = BRAND; return true;
        case 42: out = RACING; return true;
    }
    return false;
}
