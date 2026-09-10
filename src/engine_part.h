#pragma once
#include "parsers/libp.h"
#include "part.h"
#include <algorithm>

// The engine a car has by virtue of its SR_data spec class alone. CarEngine_t
// is brand-major with the three tiers (V6, small V8, big V8) in order, so the
// class picks the tier within the car's own brand row. A car still on its
// spec never had a concrete EnginePart fitted - CarBuilder leaves a
// placeholder V6 in place - so the class, not the part, is the truth.
inline CarEngine_t SpecEngineType(CarVendor_t vendor, int engineClass) {
    const int base = vendor == CarVendorFord       ? CarEngineFord_V6
                     : vendor == CarVendorChrysler ? CarEngineChrysler_V6
                                                   : CarEngineGM_V6;
    return static_cast<CarEngine_t>(base + std::clamp(engineClass, 0, 2));
}

// The hood scoop comes with the top-tier engine only - class 2, i.e. the
// 327/351/340 - not with any V8. The original reads the fitted engine part's
// class off the master parts table and ORs bit 3 of the car's body byte when
// it is 2 (LAB_109A, feeding the car draw routine's mods argument); the racer
// pool does the same from the spec's config word. Reads the engine type
// rather than a fitted EnginePart so a car still on its spec answers too -
// see Car::HasScoop.
inline bool EngineTypeHasScoop(CarEngine_t engineType) {
    return static_cast<int>(engineType) % 3 == 2;
}

struct EnginePart {
    CarEngine_t engineType;
    int wear;
    int cost;
    CarVendor_t vendor;
    AmigaLibAsset assetId;
    
    // Engine performance characteristics
    int horsepower;
    int torque;
    int displacement; // cubic inches

    // Original newspaper classified-ad name, set when bought from the
    // newspaper. Empty for engines that never came from a classified ad.
    std::string name;

    EnginePart() : engineType(CarEngineGM_V8_327), wear(0), cost(0), vendor(CarVendorGM), assetId(ENGINE_V8), horsepower(300), torque(360), displacement(327) {}
    EnginePart(CarEngine_t type, int w, int c, CarVendor_t v, AmigaLibAsset asset) 
        : engineType(type), wear(w), cost(c), vendor(v), assetId(asset) {
        // Set performance characteristics based on engine type
        switch (type) {
            case CarEngineGM_V6:
                horsepower = 180; torque = 220; displacement = 200;
                break;
            case CarEngineGM_V8_282:
                horsepower = 250; torque = 300; displacement = 282;
                break;
            case CarEngineGM_V8_327:
                horsepower = 300; torque = 360; displacement = 327;
                break;
            case CarEngineFord_V6:
                horsepower = 190; torque = 230; displacement = 200;
                break;
            case CarEngineFord_V8_292:
                horsepower = 260; torque = 310; displacement = 292;
                break;
            case CarEngineFord_V8_351:
                horsepower = 320; torque = 380; displacement = 351;
                break;
            case CarEngineChrysler_V6:
                horsepower = 185; torque = 225; displacement = 200;
                break;
            case CarEngineChrysler_V8_285:
                horsepower = 255; torque = 305; displacement = 285;
                break;
            case CarEngineChrysler_V8_340:
                horsepower = 290; torque = 340; displacement = 340;
                break;
            default:
                horsepower = 300; torque = 360; displacement = 327;
                break;
        }
    }
    
    // Convert to Part for compatibility
    Part ToPart() const {
        // For engines, we don't have a specific part type, so use transmission as default
        return Part(wear, cost, vendor, assetId, TRANS_4SPD_UNIVERSAL);
    }
    
    // Create from Part (uses vendor to determine engine type)
    static EnginePart FromPart(const Part& part) {
        // Use vendor to determine appropriate engine type
        CarEngine_t engineType;
        switch (part.vendor) {
            case CarVendorGM:
                engineType = CarEngineGM_V8_327; // Default GM engine
                break;
            case CarVendorFord:
                engineType = CarEngineFord_V8_351; // Default Ford engine
                break;
            case CarVendorChrysler:
                engineType = CarEngineChrysler_V8_340; // Default Chrysler engine
                break;
            default:
                engineType = CarEngineGM_V8_327; // Fallback
                break;
        }
        return EnginePart(engineType, part.wear, part.cost, part.vendor, part.assetId);
    }
    
    // Get engine name string
    std::string GetEngineName() const {
        switch (engineType) {
            case CarEngineChrysler_V6: return "Chrysler V6";
            case CarEngineChrysler_V8_285: return "Chrysler V8 285";
            case CarEngineChrysler_V8_340: return "Chrysler V8 340";
            case CarEngineGM_V6: return "GM V6";
            case CarEngineGM_V8_282: return "GM V8 282";
            case CarEngineGM_V8_327: return "GM V8 327";
            case CarEngineFord_V6: return "Ford V6";
            case CarEngineFord_V8_292: return "Ford V8 292";
            case CarEngineFord_V8_351: return "Ford V8 351";
            default: return "Unknown Engine";
        }
    }
    
    // Scoop comes automatically with V8s
    bool HasScoop() const { return EngineTypeHasScoop(engineType); }
    
    // Get vendor from engine type
    CarVendor_t GetVendor() const {
        switch (engineType) {
            case CarEngineChrysler_V6:
            case CarEngineChrysler_V8_285:
            case CarEngineChrysler_V8_340:
                return CarVendorChrysler;
            case CarEngineGM_V6:
            case CarEngineGM_V8_282:
            case CarEngineGM_V8_327:
                return CarVendorGM;
            case CarEngineFord_V6:
            case CarEngineFord_V8_292:
            case CarEngineFord_V8_351:
                return CarVendorFord;
            default:
                return CarVendorUnknown;
        }
    }
};
