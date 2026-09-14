#include "core/log.h"
#include "car/car.h"
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

// The SR_data engine class is the master parts table's engine tier: the nine
// engines are three tiers by three brands, and the ad text names them "V-6",
// "V-8 283 cu.in." and "V-8 327 cu.in." (docs/formats/parts_catalog.md).
CarEngine_t Car::GetEngineType() const {
    if (specEngineClass >= 0) {
        // Hardcoding GM here gave a Ford's spec engine GM displacement.
        CarVendor_t vendor = model.vendor;
        if (vendor == CarVendorUnknown) vendor = SrLibQueryGetCarVendor(model.index);
        return SpecEngineType(vendor, specEngineClass);
    }
    return engine.engineType;
}

// The scoring tier of the engine in the car: the spec's own class while the
// spec engine is still in, otherwise the fitted part's tier. CarEngine_t runs
// brand-major with the three tiers in order, so a part's tier is its index
// mod 3 - which is how the original reads it off the parts table (+2).
int Car::GetEngineClass() const {
    if (specEngineClass >= 0) return specEngineClass;
    return static_cast<int>(engine.engineType) % 3;
}

// A car loaded from its SR_data spec (i.e. never had this part explicitly
// fitted - see specManifoldClass above) never gets a concrete manifold Part;
// the default-constructed one's fields are garbage for this purpose (Part's
// union field reads back as whichever enum bit-pattern TRANS_4SPD_UNIVERSAL
// happens to be), so resolve from the spec class instead.
ManifoldType Car::GetManifoldType() const {
    if (specManifoldClass >= 0) return ManifoldTypeForClass(specManifoldClass);
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

void Car::StripEngineAssembly() {
    engineFitted = false;
    manifoldFitted = false;
    for (int i = 0; i < kMaxCarburetors; i++) carbFitted[i] = false;
    RecalculatePerformance();
}

void Car::AddEngineWearRaw(int raw) { ApplyWearRaw(engine.wear, raw); }
void Car::AddGearboxWearRaw(int raw) { ApplyWearRaw(gearbox.wear, raw); }
void Car::AddTireWearRaw(int raw) {
    ApplyWearRaw(tires.wear, raw);
    RecalculatePerformance();
}

int Car::GetPartsValue() const {
    int total = tires.cost;
    if (gearboxFitted) total += gearbox.cost;
    if (engineFitted) total += engine.cost;
    if (manifoldFitted) total += manifold.cost;
    for (int i = 0; i < kMaxCarburetors; i++) {
        if (carbFitted[i]) total += carburetors[i].cost;
    }
    return total;
}

int Car::GetStrippedValue() const {
    return std::max(0, GetValue() - (GetPartsValue() - tires.cost));
}

// The original won't move a car whose engine is apart. Every mount the
// manifold has must be filled, so a triple-2bbl with one carb still off is
// just as dead as one with no block in it.
bool Car::IsRunning() const {
    if (!engineFitted || !manifoldFitted || !gearboxFitted) return false;
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
    setup.engineClass = car.GetEngineClass();

    if (car.GetSpecManifoldClass() >= 0) {
        setup.manifoldClass = car.GetSpecManifoldClass();
    } else {
        setup.manifoldClass = ManifoldClass(car.GetManifold().partType.manifoldType);
    }

    switch (car.GetWheelType()) {
        case STOCK: setup.tireClass = 0; break;
        case BRAND: setup.tireClass = 1; break;
        case RACING: setup.tireClass = 2; break;
        default: setup.tireClass = 0; break;
    }

    setup.weightClass = std::clamp(car.GetWeightClass(), 0, 8);
    // |distance from perfect|, which over the original's -8..+4 knob is just
    // |knob| - see car_system.h's kPerfectTuneLevel.
    setup.tuneOffset = std::clamp(std::abs(car.GetTuneLevel() - kPerfectTuneLevel), 0, 8);
    // Part::wear is RodLegacy's 0..100 percent (state.cpp divides the save
    // file's raw value by 100 on load); the race sim works in the original's
    // 0..10000, which is what every threshold in PlayerCarPhysics is written
    // against - `divs.w #$3e8` for the redline hold limit, `cmpi.w #$26ac` for
    // the wear blow, `divs.w #$64` for the money-shift odds. Passing the
    // percentage straight through left all three effectively pinned at the
    // healthiest end: 15 - wear/1000 was always 15, and neither 9,900 test
    // could ever be reached. Scale it back up.
    setup.carbWear = std::clamp(car.GetCarburator().wear, 0, WEAR_MAX);
    setup.engineWear = std::clamp(car.GetEngine().wear, 0, WEAR_MAX);
    setup.transWear = std::clamp(car.GetGearbox().wear, 0, WEAR_MAX);
    setup.tireWear = std::clamp(car.GetTires().wear, 0, WEAR_MAX);

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

