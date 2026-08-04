#pragma once

#include <string>
#include "parsers/libp.h"

// Logical part types (not graphics assets)
enum TransmissionType {
    TRANS_AUTO_GM,
    TRANS_AUTO_FORD,
    TRANS_AUTO_CHRYSLER,
    TRANS_3SPD_GM,
    TRANS_3SPD_FORD,
    TRANS_3SPD_CHRYSLER,
    TRANS_4SPD_GM,
    TRANS_4SPD_FORD,
    TRANS_4SPD_CHRYSLER,
    TRANS_RACING_GM,
    TRANS_RACING_FORD,
    TRANS_RACING_CHRYSLER,
    TRANS_4SPD_UNIVERSAL
};

enum ManifoldType {
    MANIFOLD_STOCK,
    MANIFOLD_PERFORMANCE,
    MANIFOLD_RACING
};

enum CarburetorType {
    CARB_2BARREL,
    CARB_4BARREL,
    CARB_4BARREL_RACING
};

enum CarWheelType { STOCK, BRAND, RACING };

enum CarModificationType {
    MOD_ROOF_CHOP,
    MOD_BUMPER_STRIP,
    MOD_DECAL_ADD
};

// Car modification constraints
struct CarModificationConstraints {
    bool canChopRoof = true;
    bool canStripBumpers = true;
    bool canAddDecal = true;

    std::string constraintReason = "";

    bool CanModify(CarModificationType type) const {
        switch (type) {
            case MOD_ROOF_CHOP: return canChopRoof;
            case MOD_BUMPER_STRIP: return canStripBumpers;
            case MOD_DECAL_ADD: return canAddDecal;
            default: return true;
        }
    }
};

// Parts compatibility manager
class PartsCompatibilityManager {
public:
    // Check if transmission fits with engine
    static bool IsTransmissionCompatible(TransmissionType transmission, CarEngine_t engine);

    // Check if manifold fits with engine
    static bool IsManifoldCompatible(ManifoldType manifold, CarEngine_t engine);

    // Check if carburetor fits with engine (V6 → 2-brl only; V8 → 4-brl or racing)
    static bool IsCarburetorCompatible(CarburetorType carburetor, CarEngine_t engine);

    // Check if carburetor fits with manifold
    // STOCK/PERFORMANCE manifolds take 2-brl; RACING manifold takes 4-brl or racing
    static bool IsCarburetorCompatible(CarburetorType carburetor, ManifoldType manifold);

    // Get compatible parts for engine
    static std::vector<TransmissionType> GetCompatibleTransmissions(CarEngine_t engine);
    static std::vector<ManifoldType> GetCompatibleManifolds(CarEngine_t engine);
    static std::vector<CarburetorType> GetCompatibleCarburetors(CarEngine_t engine);
    static std::vector<CarburetorType> GetCompatibleCarburetors(ManifoldType manifold);
};
