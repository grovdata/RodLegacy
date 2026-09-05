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

// ============================================================================
// Fitted parts
// ============================================================================

// Unlike manifold, the raw SR_data engine class (0..3) has no confirmed
// mapping to a specific CarEngine_t - it's a scoring tier, not a documented
// body-shape selector (docs/formats/sr_data.md just calls it "engine class
// 0..3"), and CarSetupFromCar's own derivation collapses V8/V8_BIG to the
// same scoring class in the other direction, so it can't be inverted either.
// This is a guess (weakest->V6, strongest->V8_BIG): flag if a spec-based
// car's engine body looks wrong.
CarEngine_t Car::GetEngineType() const {
    if (specEngineClass >= 0) {
        switch (specEngineClass) {
            case 0: return CarEngineGM_V6;
            case 1:
            case 2: return CarEngineGM_V8_282;
            default: return CarEngineGM_V8_327;
        }
    }
    return engine.engineType;
}

// A car loaded from its SR_data spec (i.e. never had this part explicitly
// fitted - see specManifoldClass above) never gets a concrete manifold Part;
// the default-constructed one's fields are garbage for this purpose (Part's
// union field reads back as whichever enum bit-pattern TRANS_4SPD_UNIVERSAL
// happens to be), so resolve from the spec class instead.
ManifoldType Car::GetManifoldType() const {
    switch (specManifoldClass) {
        case 0: return MANIFOLD_2BARREL;
        case 1: return MANIFOLD_4BARREL;
        case 2: return MANIFOLD_DUAL_QUAD;
        case 3: return MANIFOLD_TRIPLE_2BARREL;
        case 4: return MANIFOLD_RACING;
        default: break;
    }
    return manifold.partType.manifoldType;
}

// The first fitted carburetor, or - while the car is apart - the last one
// that was in mount 0. A default-constructed Part is no good here: its union
// reads back as whatever bit pattern another enum happens to have, and
// callers index tables with the carburetor type.
Part Car::GetCarburator() const {
    for (int i = 0; i < kMaxCarburetors; i++) {
        if (carbFitted[i]) return carburetors[i];
    }
    return carburetors[0];
}

int Car::FittedCarburetorCount() const {
    int n = 0;
    for (int i = 0; i < kMaxCarburetors; i++) {
        if (carbFitted[i]) n++;
    }
    return n;
}

void Car::SetCarburator(Part carburator) {
    for (int i = 0; i < kMaxCarburetors; i++) carburetors[i] = carburator;
    RefitCarburetorMounts();
}

// Every mount on an assembled car carries the same carburetor - confirmed by
// a real save, where a triple-2bbl car holds the identical entry in all three
// slots. Re-run whenever the manifold changes, since it decides how many
// mounts there are, and because the car builders fit the carb before they
// know the manifold. The garage clears the mounts again straight after
// installing a bare manifold, so they get filled one at a time there.
void Car::RefitCarburetorMounts() {
    int slots = GetCarburetorSlots();
    Part fitted = GetCarburator();
    for (int i = 0; i < kMaxCarburetors; i++) {
        if (i < slots && !carbFitted[i]) carburetors[i] = fitted;
        carbFitted[i] = i < slots;
    }
    RecalculatePerformance();
}

void Car::SetCarburetor(int slot, Part carburator) {
    if (slot < 0 || slot >= kMaxCarburetors) return;
    carburetors[slot] = carburator;
    carbFitted[slot] = true;
    RecalculatePerformance();
}

void Car::RemoveCarburetor(int slot) {
    if (slot < 0 || slot >= kMaxCarburetors) return;
    carbFitted[slot] = false;
    RecalculatePerformance();
}

// The original won't move a car whose engine is apart. Every mount the
// manifold has must be filled, so a triple-2bbl with one carb still off is
// just as dead as one with no block in it.
bool Car::IsRunning() const {
    if (!engineFitted || !manifoldFitted) return false;
    int slots = GetCarburetorSlots();
    for (int i = 0; i < slots; i++) {
        if (!carbFitted[i]) return false;
    }
    return true;
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

