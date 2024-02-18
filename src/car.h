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
        bool isFrontBumperChopped;
        bool isBackBumperChopped;
        bool isRoofChopped;
        bool isMoving;
        int tuning;
        CarWheelType tires;
        CarModel model;

    public:
        Car(CarModel model);
        ~Car();


        CarModel GetModel();
};
