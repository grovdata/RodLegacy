#include "car/car_system.h"
#include <algorithm>

// The original gates every engine bay part on make first. A car with no known make takes anything.
bool PartsCompatibilityManager::IsVendorCompatible(CarVendor_t part, CarVendor_t car) {
    if (car == CarVendorCustom || car == CarVendorUnknown) return true;
    if (part == CarVendorCustom || part == CarVendorUnknown) return true;
    return part == car;
}

int ManifoldClass(ManifoldType type) {
    switch (type) {
        case MANIFOLD_2BARREL: return 0;
        case MANIFOLD_4BARREL: return 1;
        case MANIFOLD_DUAL_QUAD: return 3;
        case MANIFOLD_TRIPLE_2BARREL: return 2;
        case MANIFOLD_RACING: return 4;
        case MANIFOLD_TUNNEL_RAM: return 5;
        case MANIFOLD_DUAL_TUNNEL_RAM: return 6;
        case MANIFOLD_FUEL_INJECTION: return 7;
        case MANIFOLD_SUPERCHARGER: return 8;
    }
    return 0;
}

ManifoldType ManifoldTypeForClass(int manifoldClass) {
    switch (manifoldClass) {
        case 1: return MANIFOLD_4BARREL;
        case 2: return MANIFOLD_TRIPLE_2BARREL;
        case 3: return MANIFOLD_DUAL_QUAD;
        case 4: return MANIFOLD_RACING;
        case 5: return MANIFOLD_TUNNEL_RAM;
        case 6: return MANIFOLD_DUAL_TUNNEL_RAM;
        case 7: return MANIFOLD_FUEL_INJECTION;
        case 8: return MANIFOLD_SUPERCHARGER;
        default: return MANIFOLD_2BARREL;
    }
}

int CarburetorsPerManifold(ManifoldType type) {
    // SR2's table at 0x263860: injection and supercharger need no carburettor.
    static const int kPerClass[9] = {1, 1, 3, 2, 2, 1, 2, 0, 0};
    return kPerClass[ManifoldClass(type)];
}

bool PartsCompatibilityManager::IsTransmissionCompatible(TransmissionType transmission, CarEngine_t engine) {
    auto compatible = GetCompatibleTransmissions(engine);
    return std::find(compatible.begin(), compatible.end(), transmission) != compatible.end();
}

bool PartsCompatibilityManager::IsManifoldCompatible(ManifoldType manifold, CarEngine_t engine) {
    auto compatible = GetCompatibleManifolds(engine);
    return std::find(compatible.begin(), compatible.end(), manifold) != compatible.end();
}

bool PartsCompatibilityManager::IsCarburetorCompatible(CarburetorType carburetor, CarEngine_t engine) {
    auto compatible = GetCompatibleCarburetors(engine);
    return std::find(compatible.begin(), compatible.end(), carburetor) != compatible.end();
}

bool PartsCompatibilityManager::IsCarburetorCompatible(CarburetorType carburetor, ManifoldType manifold) {
    auto compatible = GetCompatibleCarburetors(manifold);
    return std::find(compatible.begin(), compatible.end(), carburetor) != compatible.end();
}

std::vector<TransmissionType> PartsCompatibilityManager::GetCompatibleTransmissions(CarEngine_t engine) {
    switch (engine) {
        case CarEngineGM_V6:
        case CarEngineGM_V8_282:
        case CarEngineGM_V8_327:
            return {TRANS_AUTO_GM, TRANS_3SPD_GM, TRANS_4SPD_GM, TRANS_RACING_GM};
        case CarEngineFord_V6:
        case CarEngineFord_V8_292:
        case CarEngineFord_V8_351:
            return {TRANS_AUTO_FORD, TRANS_3SPD_FORD, TRANS_4SPD_FORD, TRANS_RACING_FORD};
        case CarEngineChrysler_V6:
        case CarEngineChrysler_V8_285:
        case CarEngineChrysler_V8_340:
            return {TRANS_AUTO_CHRYSLER, TRANS_3SPD_CHRYSLER, TRANS_4SPD_CHRYSLER, TRANS_RACING_CHRYSLER};
        default:
            return {TRANS_4SPD_UNIVERSAL};
    }
}

// The original's fitment table, [engine tier][manifold class]:
//
//   V6      1 0 1 0 0 0 0 0 0
//   V8      1 1 0 1 1 1 1 1 1
//   V8 big  1 1 0 1 1 1 1 1 1     (cols: 2brl, 4brl, triple, dual quad, racing, then SR2's tunnel ram, dual tunnel
//                                  ram, fuel injection, supercharger; SR2 data 0x3ee74)
std::vector<ManifoldType> PartsCompatibilityManager::GetCompatibleManifolds(CarEngine_t engine) {
    switch (engine) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return {MANIFOLD_2BARREL, MANIFOLD_TRIPLE_2BARREL};
        default: // V8s, small block and big
            return {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_DUAL_QUAD, MANIFOLD_RACING, MANIFOLD_TUNNEL_RAM,
                    MANIFOLD_DUAL_TUNNEL_RAM, MANIFOLD_FUEL_INJECTION, MANIFOLD_SUPERCHARGER};
    }
}

std::vector<CarburetorType> PartsCompatibilityManager::GetCompatibleCarburetors(CarEngine_t engine) {
    switch (engine) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return {CARB_2BARREL};
        default: // V8s
            return {CARB_4BARREL, CARB_4BARREL_RACING};
    }
}

// The original's carb fitment table, [manifold class][carb type]:
//
//   2brl        1 0 0
//   4brl        0 1 1
//   triple      1 0 0
//   dual quad   0 1 1
//   racing      0 1 1     (cols: 2brl, 4brl, racing 4brl)
//   tunnel ram, dual tunnel ram   0 1 1;  fuel injection, supercharger   0 0 0   (SR2 data 0x3ee8f)
std::vector<CarburetorType> PartsCompatibilityManager::GetCompatibleCarburetors(ManifoldType manifold) {
    switch (manifold) {
        case MANIFOLD_2BARREL:
        case MANIFOLD_TRIPLE_2BARREL: // three 2-barrels
            return {CARB_2BARREL};
        case MANIFOLD_4BARREL:
        case MANIFOLD_DUAL_QUAD:
        case MANIFOLD_RACING:
        case MANIFOLD_TUNNEL_RAM:
        case MANIFOLD_DUAL_TUNNEL_RAM:
            return {CARB_4BARREL, CARB_4BARREL_RACING};
        default: // fuel injection and supercharger take none
            return {};
    }
}

int Sr2DriftTune(int tune, std::mt19937& gen) {
    auto rnd = [&gen](int n) { return std::uniform_int_distribution<int>(0, n - 1)(gen); };
    if (rnd(3) <= 0) return tune;
    if (tune > 0) return rnd(127) > tune ? rnd(127) : tune;
    if (tune < 0) return -rnd(128) > tune ? tune : -rnd(128);
    return rnd(254) - 127;
}
