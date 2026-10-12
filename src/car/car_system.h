/*
 * RodLegacy - Part types and fitment: transmissions, manifolds, carburetors, wheels, and
 * PartsCompatibilityManager.
 *
 * Created: 2025-10-20
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <random>
#include <string>
#include "parsers/libp.h"

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
    MANIFOLD_2BARREL,
    MANIFOLD_4BARREL,
    MANIFOLD_DUAL_QUAD,
    MANIFOLD_TRIPLE_2BARREL,
    MANIFOLD_RACING,
    // Street Rod 2 only (classes 5..8).
    MANIFOLD_TUNNEL_RAM,
    MANIFOLD_DUAL_TUNNEL_RAM,
    MANIFOLD_FUEL_INJECTION,
    MANIFOLD_SUPERCHARGER
};

enum CarburetorType {
    CARB_2BARREL,
    CARB_4BARREL,
    CARB_4BARREL_RACING
};

// Not ManifoldType's order: the original has triple 2-barrel at class 2 and dual quad at 3.
int ManifoldClass(ManifoldType type);

// Its inverse, for reading a class back out of SR_data. Keep the two together:
// hand-rolling this mapping is what put a V6-only triple-2barrel on the big
// V8s, whose spec manifold class is 3 (dual quad), not 2.
ManifoldType ManifoldTypeForClass(int manifoldClass);

// Carburetors per manifold: 1, 1, 3, 2, 2. Each is a part in its own right.
int CarburetorsPerManifold(ManifoldType type);
constexpr int kMaxCarburetors = 3;

// 13 positions. The original stores -8..+4 signed; here unsigned, so perfect is 8. It is re-rolled after
// every engine bay part change.
constexpr int kTuneLevelCount = 13;
constexpr int kPerfectTuneLevel = 8;

// Street Rod 2's tune after a race (lmech.c 0x259418): two times in three the signed byte wanders, the way the
// original's random calls do. rand(n) is uniform in 0..n-1.
int Sr2DriftTune(int tune, std::mt19937& gen);

enum CarWheelType { STOCK, BRAND, RACING };

enum CarModificationType {
    MOD_ROOF_CHOP,
    MOD_BUMPER_STRIP,
    MOD_DECAL_ADD
};

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

namespace PartsCompatibilityManager {
    bool IsTransmissionCompatible(TransmissionType transmission, CarEngine_t engine);

    bool IsManifoldCompatible(ManifoldType manifold, CarEngine_t engine);

    // Check if carburetor fits with engine (V6 -> 2-brl only; V8 -> 4-brl or racing)
    bool IsCarburetorCompatible(CarburetorType carburetor, CarEngine_t engine);

    // Check if carburetor fits with manifold
    // STOCK/PERFORMANCE manifolds take 2-brl; RACING manifold takes 4-brl or racing
    bool IsCarburetorCompatible(CarburetorType carburetor, ManifoldType manifold);
    // Make must match before any of the above is even asked - see .cpp.
    bool IsVendorCompatible(CarVendor_t part, CarVendor_t car);

    std::vector<TransmissionType> GetCompatibleTransmissions(CarEngine_t engine);
    std::vector<ManifoldType> GetCompatibleManifolds(CarEngine_t engine);
    std::vector<CarburetorType> GetCompatibleCarburetors(CarEngine_t engine);
    std::vector<CarburetorType> GetCompatibleCarburetors(ManifoldType manifold);
}
