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
}

Car::~Car() {

}

CarEngine_t Car::GetEngineType() const {
    if (specEngineClass >= 0) {
        // Hardcoding GM here gave a Ford's spec engine GM displacement.
        CarVendor_t vendor = model.vendor;
        if (vendor == CarVendorUnknown) vendor = SrLibQueryGetCarVendor(model.index);
        return SpecEngineType(vendor, specEngineClass);
    }
    return engine.engineType;
}

// The spec's class while the spec engine is in, otherwise the fitted part's tier (index mod 3).
int Car::GetEngineClass() const {
    if (specEngineClass >= 0) return specEngineClass;
    return static_cast<int>(engine.engineType) % 3;
}

// A car on its SR_data spec has no concrete manifold Part, and the default one's union reads as garbage.
ManifoldType Car::GetManifoldType() const {
    if (specManifoldClass >= 0) return ManifoldTypeForClass(specManifoldClass);
    return manifold.partType.manifoldType;
}

// The first fitted carburetor, or the last one in mount 0 while the car is apart.
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

// Every mount on an assembled car carries the same carburetor. Re-run on manifold change, since it decides
// the mount count.
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

// Every mount the manifold has must be filled.
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
    // The lookup only knows lib.p assets.
    CarVendor_t vendor = model.vendor;
    if (vendor == CarVendorUnknown) {
        vendor = SrLibQueryGetCarVendor(model.index);
    }
    const AmigaLibAsset aid = TireAssetForType(type);

    LOG_INFO("Setting tires for %d to %d", model.index, aid);

    this->tires = Part(wear, cost, vendor, aid, type);
    RecalculatePerformance();
}

// Top speed is ComputeBasePerformance's score * 13/10, capped at 130 mph.

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

    setup.exhaust = car.GetExhaust();
    setup.muffler = car.GetMuffler();
    setup.mufflerRemoved = car.IsMufflerRemoved();
    setup.differential = car.GetDifferential();

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
    setup.sr2Tune = car.GetSr2Tune();
    setup.carbWear = std::clamp(car.GetCarburator().wear, 0, WEAR_MAX);
    setup.engineWear = std::clamp(car.GetEngine().wear, 0, WEAR_MAX);
    setup.transWear = std::clamp(car.GetGearbox().wear, 0, WEAR_MAX);
    setup.tireWear = std::clamp(car.GetTires().wear, 0, WEAR_MAX);

    // A body that cannot take a mod scores as if it were done, as in the original: the 1963 Corvette's
    // real 119 mph depends on it. CarModel::constraints is never populated, so read the sprite tables.
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

