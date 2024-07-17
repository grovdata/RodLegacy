#include "car.h"
#include "parsers/libp.h"

Car::Car(CarModel model) {
    this->model = model;
}

Car::~Car() {

}

CarModel Car::GetModel() {
    return model;
}

CarWheelType Car::GetTireType() {
    return tires;
}

int Car::GetDriver() {
    return DRIVER_04;
}

CarDecal Car::GetDecal() {
    return CarDecalHotLips;
}