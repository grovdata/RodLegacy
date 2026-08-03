#include "log.h"
#include "car.h"
#include "parsers/libp.h"

Car::Car() {
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
    effectiveSpeed = 0;
    effectiveAcceleration = 0;
}

Car::Car(CarModel model) {
    this->model = model;
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
    effectiveSpeed = model.baseSpeed;
    effectiveAcceleration = model.baseAcceleration;
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

void Car::SetTires(CarWheelType type) {
    CarVendor_t vendor = SrLibQueryGetCarVendor(model.index);
    AmigaLibAsset aid;
    switch (type) {
        case STOCK:
            aid = CAR_TIRES_STOCK;
            break;
        case BRAND:
            aid = CAR_TIRES_BRAND;
            break;
        case RACING:
            aid = CAR_TIRES_RACING;
            break;
    }

    LOG_INFO("Setting tires for %d to %d", model.index, aid);

    this->tires = Part(0, 0, vendor, aid, TRANS_4SPD_UNIVERSAL);
}

void Car::SetScoop(CarScoopType type) {
    this->scoop = type;
}

void Car::RecalculatePerformance() {
    // Base car performance
    effectiveSpeed = model.baseSpeed;
    effectiveAcceleration = model.baseAcceleration;

    // Engine contribution (based on horsepower and wear)
    int engineSpeedBonus = (engine.horsepower * (100 - engine.wear)) / 100;
    effectiveSpeed += engineSpeedBonus;

    // Transmission contribution (based on type and wear)
    int transSpeedBonus = GetTransmissionSpeedBonus(gearbox.partType.transmissionType, gearbox.wear);
    effectiveSpeed += transSpeedBonus;

    // Manifold contribution (based on type and wear)
    int manifoldSpeedBonus = GetManifoldSpeedBonus(manifold.partType.manifoldType, manifold.wear);
    effectiveSpeed += manifoldSpeedBonus;

    // Carburetor contribution (based on type and wear)
    int carbSpeedBonus = GetCarburetorSpeedBonus(carburator.partType.carburetorType, carburator.wear);
    effectiveSpeed += carbSpeedBonus;

    // Modification bonuses
    if (isRoofChopped) {
        effectiveSpeed += 5; // Simple bonus
    }
    if (isFrontBumperStripped && isBackBumperStripped) {
        effectiveSpeed += 3;
    }

    // Scoop bonus is automatic with V8 engines
    if (engine.HasScoop()) {
        effectiveSpeed += 2; // Small bonus for V8 scoop
    }
}

// Helper methods for part speed bonuses
int Car::GetTransmissionSpeedBonus(TransmissionType type, int wear) const {
    int baseBonus = 0;
    switch (type) {
        case TRANS_RACING_GM:
        case TRANS_RACING_FORD:
        case TRANS_RACING_CHRYSLER:
            baseBonus = 15;
            break;
        case TRANS_4SPD_GM:
        case TRANS_4SPD_FORD:
        case TRANS_4SPD_CHRYSLER:
            baseBonus = 10;
            break;
        case TRANS_3SPD_GM:
        case TRANS_3SPD_FORD:
        case TRANS_3SPD_CHRYSLER:
            baseBonus = 5;
            break;
        case TRANS_AUTO_GM:
        case TRANS_AUTO_FORD:
        case TRANS_AUTO_CHRYSLER:
            baseBonus = 0;
            break;
        case TRANS_4SPD_UNIVERSAL:
            baseBonus = 8;
            break;
    }
    return (baseBonus * (100 - wear)) / 100;
}

int Car::GetManifoldSpeedBonus(ManifoldType type, int wear) const {
    int baseBonus = 0;
    switch (type) {
        case MANIFOLD_RACING:
            baseBonus = 12;
            break;
        case MANIFOLD_PERFORMANCE:
            baseBonus = 8;
            break;
        case MANIFOLD_STOCK:
            baseBonus = 0;
            break;
    }
    return (baseBonus * (100 - wear)) / 100;
}

int Car::GetCarburetorSpeedBonus(CarburetorType type, int wear) const {
    int baseBonus = 0;
    switch (type) {
        case CARB_4BARREL_RACING:
            baseBonus = 10;
            break;
        case CARB_4BARREL:
            baseBonus = 6;
            break;
        case CARB_2BARREL:
            baseBonus = 0;
            break;
    }
    return (baseBonus * (100 - wear)) / 100;
}
