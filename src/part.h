#pragma once
#include "parsers/libp.h"
#include "car_system.h"
#include <string>

// Which spare-parts category a part belongs to. Used to route a newspaper
// purchase to the matching State::AddSpare* accessor, and to pick the sale
// formula below.
enum class PartCategory { Engine, Transmission, Carburetor, Manifold, Tire };

// What a spare part fetches at the garage: one take-it-or-leave-it price,
// from SellSparePartsLoop (Amiga 0x0024284a). The category jump table there
// pairs engine with transmission and carburettor with manifold; tires are
// always worthless. Full write-up in RodLegacyResearch/docs/gameplay.md
// ("Selling").
//
// wear is RodLegacy's 0..100 percent, where the original keeps 0..10000 and
// divides by 100 to get the same number.
inline int PartSalePrice(PartCategory category, int value, int wear, int roll20) {
    const int condition = 100 - wear;
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

// Catalog price of a part, in dollars. The original keeps one master parts
// table (Amiga A4-0x48ec, 44 records of 14 bytes, price at +0) and every part
// it ever creates - a car's stock drivetrain, a newspaper ad, a replacement
// set of tires - carries that price as its value. The car's own value is built
// out of these, so a part with no price makes the car it is on unsellable.
// Records in order: 9 engines, 13 transmissions, 3 carbs, 15 manifolds,
// 3 tires. See RodLegacyResearch/docs/formats/parts_catalog.md.
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

// Priced by class, not by brand - all three brands of a given manifold class
// cost the same, and the two middle classes both cost $35.
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
    AmigaLibAsset assetId; // Only for graphics

    // Original newspaper classified-ad name (e.g. "Trnsmsn 4-spd. Ford."),
    // set when bought from the newspaper. Empty for parts that never came
    // from a classified ad (e.g. the starting car's stock parts).
    std::string name;
    
    // Logical part type (for compatibility checking)
    union {
        TransmissionType transmissionType;
        ManifoldType manifoldType;
        CarburetorType carburetorType;
        CarWheelType tireType;
    } partType;

    // Default constructor
    Part() : wear(0), cost(0), vendor(CarVendorUnknown), assetId(ENGINE_V8) {
        partType.transmissionType = TRANS_4SPD_UNIVERSAL;
    }

    // Constructor for transmission
    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, TransmissionType transType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.transmissionType = transType;
    }

    // Constructor for manifold
    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, ManifoldType manifoldType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.manifoldType = manifoldType;
    }

    // Constructor for carburetor
    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, CarburetorType carbType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.carburetorType = carbType;
    }

    // Constructor for tires
    Part(int w, int c, CarVendor_t v, AmigaLibAsset asset, CarWheelType wheelType)
        : wear(w), cost(c), vendor(v), assetId(asset) {
        partType.tireType = wheelType;
    }
};