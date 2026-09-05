#include "car_system.h"
#include <algorithm>

// The original gates every engine bay part on make before anything else:
// `carSpec.brand & part.brand & 0xF000` (Street_Rod.c's engine bay loop,
// all three slot groups). The brand is a 1/2/4 bitmask, one bit per car -
// it matches the real manufacturer of all 26 specs, so this is a plain
// equality. A car with no known make takes anything.
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
    }
    return 0;
}

int CarburetorsPerManifold(ManifoldType type) {
    static const int kPerClass[5] = {1, 1, 3, 2, 2};
    return kPerClass[ManifoldClass(type)];
}

// Parts compatibility manager implementation
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

// Straight off the original's fitment table at A4-0x4672, a 3x5 byte matrix
// of [engine tier][manifold class] - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md:
//
//   V6      1 0 1 0 0
//   V8      1 1 0 1 1
//   V8 big  1 1 0 1 1     (cols: 2brl, 4brl, triple, dual quad, racing)
//
// So a V6 takes the two 2-barrel manifolds and nothing else, and no V8 takes
// the triple.
std::vector<ManifoldType> PartsCompatibilityManager::GetCompatibleManifolds(CarEngine_t engine) {
    switch (engine) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return {MANIFOLD_2BARREL, MANIFOLD_TRIPLE_2BARREL};
        default: // V8s, small block and big
            return {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_DUAL_QUAD, MANIFOLD_RACING};
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

// The original's other fitment table, A4-0x4663, a 5x3 byte matrix of
// [manifold class][carb type]:
//
//   2brl        1 0 0
//   4brl        0 1 1
//   triple      1 0 0
//   dual quad   0 1 1
//   racing      0 1 1     (cols: 2brl, 4brl, racing 4brl)
//
// A 2-barrel manifold takes 2-barrels; everything else takes a 4-barrel of
// either kind. Note a dual quad will not take 2-barrels, and a plain
// 4-barrel manifold does accept the racing carb.
std::vector<CarburetorType> PartsCompatibilityManager::GetCompatibleCarburetors(ManifoldType manifold) {
    switch (manifold) {
        case MANIFOLD_2BARREL:
        case MANIFOLD_TRIPLE_2BARREL: // three 2-barrels
            return {CARB_2BARREL};
        case MANIFOLD_4BARREL:
        case MANIFOLD_DUAL_QUAD:
        case MANIFOLD_RACING:
            return {CARB_4BARREL, CARB_4BARREL_RACING};
        default:
            return {};
    }
}
