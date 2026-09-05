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

// Matches SR_data's manifold spec field (0..4) and the newspaper classifieds'
// 5 manifold subtypes per brand, in increasing performance/price order.
enum ManifoldType {
    MANIFOLD_2BARREL,
    MANIFOLD_4BARREL,
    MANIFOLD_DUAL_QUAD,
    MANIFOLD_TRIPLE_2BARREL,
    MANIFOLD_RACING
};

enum CarburetorType {
    CARB_2BARREL,
    CARB_4BARREL,
    CARB_4BARREL_RACING
};

// The Amiga part table's manifold class (flag bits 3-5 of the 14-byte part
// record at A4-0x48ea). Deliberately not ManifoldType's order: the original
// has triple 2-barrel at class 2 and dual quad at class 3.
int ManifoldClass(ManifoldType type);

// How many carburetors a manifold mounts (A4-0x3b9c: 1, 1, 3, 2, 2). Each is
// a part in its own right - the car struct carries three separate carburetor
// pointers, see RodLegacyResearch/docs/garage/engine_bay_layout.md.
int CarburetorsPerManifold(ManifoldType type);
constexpr int kMaxCarburetors = 3;

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

// Parts compatibility checks
namespace PartsCompatibilityManager {
    // Check if transmission fits with engine
    bool IsTransmissionCompatible(TransmissionType transmission, CarEngine_t engine);

    // Check if manifold fits with engine
    bool IsManifoldCompatible(ManifoldType manifold, CarEngine_t engine);

    // Check if carburetor fits with engine (V6 → 2-brl only; V8 → 4-brl or racing)
    bool IsCarburetorCompatible(CarburetorType carburetor, CarEngine_t engine);

    // Check if carburetor fits with manifold
    // STOCK/PERFORMANCE manifolds take 2-brl; RACING manifold takes 4-brl or racing
    bool IsCarburetorCompatible(CarburetorType carburetor, ManifoldType manifold);

    // Get compatible parts for engine
    std::vector<TransmissionType> GetCompatibleTransmissions(CarEngine_t engine);
    std::vector<ManifoldType> GetCompatibleManifolds(CarEngine_t engine);
    std::vector<CarburetorType> GetCompatibleCarburetors(CarEngine_t engine);
    std::vector<CarburetorType> GetCompatibleCarburetors(ManifoldType manifold);
}
