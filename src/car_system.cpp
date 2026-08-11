#include "car_system.h"
#include <algorithm>

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

std::vector<ManifoldType> PartsCompatibilityManager::GetCompatibleManifolds(CarEngine_t engine) {
    switch (engine) {
        // Triple-2bbl only fits V6s (confirmed in game: a GM 327 V8 rejects
        // it as an install option), not any V8, big-block or small.
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_TRIPLE_2BARREL};
        default: // V8s
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

std::vector<CarburetorType> PartsCompatibilityManager::GetCompatibleCarburetors(ManifoldType manifold) {
    // Per the newspaper classifieds' own ad text ("manifold, for 2-4brl
    // carbs" / "manifold, for 3-2brl carbs"), fitment is explicit, not
    // guessed:
    switch (manifold) {
        case MANIFOLD_2BARREL:
        case MANIFOLD_TRIPLE_2BARREL: // three 2-barrels
            return {CARB_2BARREL};
        case MANIFOLD_4BARREL:
            return {CARB_4BARREL};
        case MANIFOLD_DUAL_QUAD: // "for 2-4brl carbs"
            return {CARB_2BARREL, CARB_4BARREL};
        case MANIFOLD_RACING:
            return {CARB_4BARREL, CARB_4BARREL_RACING};
        default:
            return {};
    }
}
