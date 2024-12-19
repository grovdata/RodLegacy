#include "car.h"
#include "parsers/libp.h"

Car::Car() {

}

Car::Car(CarModel model) {
    this->model = model;    
}

Car::~Car() {
    delete this->tires;
}

CarModel Car::GetModel() {
    return model;
}

SrLibCarDecal Car::GetDecal() {
    return decal;
}

void Car::SetTires(CarWheelType type) {
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
    
    this->tires = new Part(aid);
    std::cout << "Tire asset ID: " << static_cast<int>(this->tires->GetAssetId()) << std::endl;
}

void Car::SetScoop(CarScoopType type) {
    this->scoop = type;
}