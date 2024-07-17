#pragma once

#include "global.h"
#include "parsers/libp.h"

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
        CarDecal decal;

    public:
        Car(CarModel model);
        ~Car();

        CarModel GetModel();
        CarWheelType GetTireType();
        CarDecal GetDecal();
        int GetDriver();
};