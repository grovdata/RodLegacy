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

int Car::GetDriver() {
    return DRIVER_04;
}

CarDecal Car::GetDecal() {
    return decal;
}