#include "car.h"
#include "parsers/libp.h"

Car::Car() {

}

Car::Car(CarModel model) {
    this->model = model;
    this->tires = tires;
}

Car::~Car() {

}

CarModel Car::GetModel() {
    return model;
}

CarDecal Car::GetDecal() {
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

    Part p(aid);
    this->tires = &p;
    std::cout << "Tire asset ID: " << static_cast<int>(this->tires->GetAssetId()) << std::endl;
}