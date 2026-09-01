#include "log.h"
#include "car.h"
#include "parsers/libp.h"

#include <algorithm>
#include <cstdlib>

Car::Car() {
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
    RecalculatePerformance();
}

Car::Car(CarModel model) {
    this->model = model;
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
    RecalculatePerformance();
    // Engine will be initialized by CarBuilder
}

Car::~Car() {

}

CarModel Car::GetModel() {
    return model;
}

SrLibCarDecal Car::GetDecal() {
    return decal;
}

CarWheelType Car::GetWheelType() const {
    if (tires.assetId == CAR_TIRES_RACING) return RACING;
    if (tires.assetId == CAR_TIRES_BRAND)  return BRAND;
    return STOCK;
}

AmigaLibAsset TireAssetForType(CarWheelType type) {
    switch (type) {
        case STOCK:  return CAR_TIRES_STOCK;
        case BRAND:  return CAR_TIRES_BRAND;
        case RACING: return CAR_TIRES_RACING;
    }
    return CAR_TIRES_STOCK;
}

void Car::SetTires(CarWheelType type, int wear, int cost) {
    // model.vendor is already resolved for both sources; the lookup only knows
    // lib.p assets and would answer Unknown for a Street Rod SE car.
    CarVendor_t vendor = model.vendor;
    if (vendor == CarVendorUnknown) {
        vendor = SrLibQueryGetCarVendor(model.index);
    }
    const AmigaLibAsset aid = TireAssetForType(type);

    LOG_INFO("Setting tires for %d to %d", model.index, aid);

    this->tires = Part(wear, cost, vendor, aid, type);
    RecalculatePerformance();
}

void Car::SetScoop(CarScoopType type) {
    this->scoop = type;
}

// ============================================================================
// Top speed
// The original derives it from one number: ComputeBasePerformance scores the
// installed parts, and the top speed is that score * 13/10 capped at 130 mph.
// See RodLegacyResearch/docs/driving/top_speed.md.
// ============================================================================

srrace::CarSetup CarSetupFromCar(const Car& car, bool automaticOverride) {
    srrace::CarSetup setup;

    switch (car.GetCarburator().partType.carburetorType) {
        case CARB_2BARREL: setup.carbClass = 0; break;
        case CARB_4BARREL: setup.carbClass = 1; break;
        case CARB_4BARREL_RACING: setup.carbClass = 2; break;
        default: setup.carbClass = 0; break;
    }

    switch (car.GetGearbox().partType.transmissionType) {
        case TRANS_AUTO_GM:
        case TRANS_AUTO_FORD:
        case TRANS_AUTO_CHRYSLER:
            setup.transClass = 0;
            break;
        case TRANS_3SPD_GM:
        case TRANS_3SPD_FORD:
        case TRANS_3SPD_CHRYSLER:
            setup.transClass = 1;
            break;
        case TRANS_4SPD_GM:
        case TRANS_4SPD_FORD:
        case TRANS_4SPD_CHRYSLER:
        case TRANS_4SPD_UNIVERSAL:
            setup.transClass = 2;
            break;
        case TRANS_RACING_GM:
        case TRANS_RACING_FORD:
        case TRANS_RACING_CHRYSLER:
            setup.transClass = 3;
            break;
        default:
            setup.transClass = 2;
            break;
    }

    if (automaticOverride) {
        setup.transClass = 0;
    }

    // A car that still carries its spec's engine and manifold scores on the
    // spec's own classes; once the player fits a different part the class comes
    // from that part instead.
    if (car.GetSpecEngineClass() >= 0) {
        setup.engineClass = car.GetSpecEngineClass();
    } else {
        switch (car.GetEngine().engineType) {
            case CarEngineGM_V6:
            case CarEngineFord_V6:
            case CarEngineChrysler_V6:
                setup.engineClass = 1;
                break;
            default:
                setup.engineClass = 2;
                break;
        }
    }

    if (car.GetSpecManifoldClass() >= 0) {
        setup.manifoldClass = car.GetSpecManifoldClass();
    } else {
        switch (car.GetManifold().partType.manifoldType) {
            case MANIFOLD_2BARREL: setup.manifoldClass = 0; break;
            case MANIFOLD_4BARREL: setup.manifoldClass = 1; break;
            case MANIFOLD_DUAL_QUAD: setup.manifoldClass = 2; break;
            case MANIFOLD_TRIPLE_2BARREL: setup.manifoldClass = 3; break;
            case MANIFOLD_RACING: setup.manifoldClass = 4; break;
            default: setup.manifoldClass = 0; break;
        }
    }

    switch (car.GetWheelType()) {
        case STOCK: setup.tireClass = 0; break;
        case BRAND: setup.tireClass = 1; break;
        case RACING: setup.tireClass = 2; break;
        default: setup.tireClass = 0; break;
    }

    setup.weightClass = std::clamp(car.GetWeightClass(), 0, 8);
    // Tune level runs 0..12 with 9 perfect, against the original's -8..+4 knob.
    setup.tuneOffset = std::clamp(std::abs(car.GetTuneLevel() - 9), 0, 8);
    setup.carbWear = std::clamp(car.GetCarburator().wear, 0, 10000);
    setup.engineWear = std::clamp(car.GetEngine().wear, 0, 10000);
    setup.transWear = std::clamp(car.GetGearbox().wear, 0, 10000);
    setup.tireWear = std::clamp(car.GetTires().wear, 0, 10000);

    // A body that cannot take a mod at all scores as if the mod were done: the
    // original checks the model's sprite table and hands out the points when
    // the sprite is missing, so no car is penalised for what it cannot do.
    // That freebie is worth 9 mph-points on a Corvette, which has no
    // chopped-roof sprite - it is the difference between the 1963's real
    // 119 mph and the 110 it would otherwise score.
    //
    // These read the sprite tables directly rather than CarModel::constraints,
    // which nothing ever populates (it is stuck at "everything allowed").
    const bool canChop = CarModelCanChopRoof(car.GetModelRef());
    const bool canDebumper = CarModelCanStripBumpers(car.GetModelRef());
    setup.roofChopped = car.IsRoofChopped() || !canChop;
    setup.frontBumperRemoved = car.IsFrontBumperStripped() || !canDebumper;
    setup.rearBumperRemoved = car.IsBackBumperStripped() || !canDebumper;
    return setup;
}

void Car::RecalculatePerformance() {
    topSpeed = srrace::ComputeTopSpeed(CarSetupFromCar(*this, false));
}

