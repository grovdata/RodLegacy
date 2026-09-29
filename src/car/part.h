#pragma once
#include "parsers/libp.h"
#include "car/car_system.h"
#include <algorithm>
#include <string>

// The original's unit: 0..10000, where 10000 is scrap. Only the screen divides by 100.
constexpr int WEAR_MAX = 10000;
constexpr int WEAR_PER_PERCENT = 100;
inline int WearPercent(int wear) { return std::clamp(wear, 0, WEAR_MAX) / WEAR_PER_PERCENT; }

enum class PartCategory { Engine, Transmission, Carburetor, Manifold, Tire };

// One take-it-or-leave-it price. Engine pairs with transmission, carburettor with manifold; tires are worthless.
inline int PartSalePrice(PartCategory category, int value, int wear, int roll20) {
    const int condition = 100 - WearPercent(wear);
    int price = value;
    switch (category) {
        case PartCategory::Engine:
        case PartCategory::Transmission:
            price = value * condition * 9 / 1000;
            break;
        case PartCategory::Carburetor:
        case PartCategory::Manifold:
            price = value * (70 + roll20) / 100;
            break;
        case PartCategory::Tire:
            price = 0;
            break;
    }
    return price / 5 * 5;
}

inline void ApplyWearRaw(int& wear, int raw) {
    if (raw <= 0) return;
    wear = std::clamp(wear + raw, 0, WEAR_MAX);
}

// Every part the original creates carries this as its value, so a part with no price makes its car unsellable.
// Records in order: 9 engines, 13 transmissions, 3 carbs, 15 manifolds, 3 tires.
inline int PartCatalogPrice(CarEngine_t type) {
    switch (type) {
        case CarEngineChrysler_V6:     return 100;
        case CarEngineGM_V6:           return 100;
        case CarEngineFord_V6:         return 100;
        case CarEngineGM_V8_282:       return 350;
        case CarEngineFord_V8_292:     return 340;
        case CarEngineChrysler_V8_285: return 360;
        case CarEngineGM_V8_327:       return 500;
        case CarEngineFord_V8_351:     return 475;
        case CarEngineChrysler_V8_340: return 525;
    }
    return 0;
}

inline int PartCatalogPrice(TransmissionType type) {
    switch (type) {
        case TRANS_AUTO_GM:            return 130;
        case TRANS_AUTO_FORD:          return 120;
        case TRANS_AUTO_CHRYSLER:      return 125;
        case TRANS_3SPD_GM:            return 175;
        case TRANS_3SPD_FORD:          return 160;
        case TRANS_3SPD_CHRYSLER:      return 170;
        case TRANS_4SPD_GM:            return 250;
        case TRANS_4SPD_FORD:          return 260;
        case TRANS_4SPD_CHRYSLER:      return 245;
        case TRANS_4SPD_UNIVERSAL:     return 275;
        case TRANS_RACING_GM:          return 400;
        case TRANS_RACING_FORD:        return 405;
        case TRANS_RACING_CHRYSLER:    return 410;
    }
    return 0;
}

inline int PartCatalogPrice(CarburetorType type) {
    switch (type) {
        case CARB_2BARREL:        return 25;
        case CARB_4BARREL:        return 40;
        case CARB_4BARREL_RACING: return 60;
    }
    return 0;
}

// All three brands of a class cost the same; the two middle classes both cost $35.
inline int PartCatalogPrice(ManifoldType type) {
    static const int kByClass[5] = {20, 25, 35, 35, 60};
    return kByClass[ManifoldClass(type)];
}

inline int PartCatalogPrice(CarWheelType type) {
    switch (type) {
        case STOCK:  return 50;
        case BRAND:  return 80;
        case RACING: return 150;
    }
    return 0;
}

struct Part {
    int wear;
    int cost;
    CarVendor_t vendor;
    AmigaLibAsset assetId;

    std::string name;
    
    union {
        TransmissionType transmissionType;
        ManifoldType manifoldType;
        CarburetorType carburetorType;
        CarWheelType tireType;
    } partType;

    Part() : wear(0), cost(0), vendor(CarVendorUnknown), assetId(ENGINE_V8) {
        partType.transmissionType = TRANS_4SPD_UNIVERSAL;
    }

    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, TransmissionType transType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.transmissionType = transType;
    }

    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, ManifoldType manifoldType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.manifoldType = manifoldType;
    }

    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, CarburetorType carbType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.carburetorType = carbType;
    }

    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, CarWheelType wheelType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.tireType = wheelType;
    }
};