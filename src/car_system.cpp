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
            return {TRANS_AUTO_GM, TRANS_3SPD_GM, TRANS_4SPD_GM};
        case CarEngineGM_V8_282:
        case CarEngineGM_V8_327:
            return {TRANS_AUTO_GM, TRANS_3SPD_GM, TRANS_4SPD_GM, TRANS_RACING_GM};
        case CarEngineFord_V6:
            return {TRANS_AUTO_FORD, TRANS_3SPD_FORD, TRANS_4SPD_FORD};
        case CarEngineFord_V8_292:
        case CarEngineFord_V8_351:
            return {TRANS_AUTO_FORD, TRANS_3SPD_FORD, TRANS_4SPD_FORD, TRANS_RACING_FORD};
        case CarEngineChrysler_V6:
            return {TRANS_AUTO_CHRYSLER, TRANS_3SPD_CHRYSLER, TRANS_4SPD_CHRYSLER};
        case CarEngineChrysler_V8_285:
        case CarEngineChrysler_V8_340:
            return {TRANS_AUTO_CHRYSLER, TRANS_3SPD_CHRYSLER, TRANS_4SPD_CHRYSLER, TRANS_RACING_CHRYSLER};
        default:
            return {TRANS_4SPD_UNIVERSAL};
    }
}

std::vector<ManifoldType> PartsCompatibilityManager::GetCompatibleManifolds(CarEngine_t engine) {
    switch (engine) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return {MANIFOLD_STOCK, MANIFOLD_PERFORMANCE};
        default: // V8s
            return {MANIFOLD_STOCK, MANIFOLD_RACING};
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
    switch (manifold) {
        case MANIFOLD_STOCK:
        case MANIFOLD_PERFORMANCE:
            // 2-brl and 3-2brl manifolds take 2-brl carbs
            return {CARB_2BARREL};
        case MANIFOLD_RACING:
            // 4-brl and racing manifolds share the same fitting
            return {CARB_4BARREL, CARB_4BARREL_RACING};
        default:
            return {};
    }
}
