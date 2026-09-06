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