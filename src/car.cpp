#include "car.h"
#include "parsers/libp.h"

Car::Car() {
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
}

Car::Car(CarModel model) {
    this->model = model;
    isFrontBumperStripped = false;
    isBackBumperStripped = false;
    isRoofChopped = false;
}

Car::~Car() {
   
}

CarModel Car::GetModel() {
    return model;
}

SrLibCarDecal Car::GetDecal() {
    return decal;
}

void Car::SetTires(CarWheelType type) {
    CarVendor_t vendor = (CarVendor_t)CAR_VENDOR_LOOKUP[model.index][1];
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

    SDL_Log("Setting tires for %d to %d", model.index, aid);
    
    this->tires = Part{0, 0, vendor, aid};
}

void Car::SetScoop(CarScoopType type) {
    this->scoop = type;
}