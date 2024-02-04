#include "car.h"

Car::Car(CarModel model) {
    this->model = model;
}

Car::~Car() {

}

CarModel Car::GetModel() {
    return model;
}