#pragma once

#include "global.h"

class Spritesheet;
extern SDL_Renderer *renderer;

enum CarWheelType { STOCK, BRAND, RACING };

class Car {
    /*
        Car positions
         Garage: 42, 82
         Diner :
         Gas station:
    */
    private:
        [[maybe_unused]] bool isFrontBumperChopped;
        [[maybe_unused]] bool isBackBumperChopped;
        [[maybe_unused]] bool isRoofChopped;
        [[maybe_unused]] bool isMoving;
        [[maybe_unused]] int tuning;
        [[maybe_unused]] CarWheelType tires;
        CarModel model;

    public:
        Car(CarModel model);
        ~Car();


        CarModel GetModel();
};
