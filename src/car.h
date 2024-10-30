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
        // Parts
        Part* gearbox;
        Part* tires;
        Part* engine;
        Part* carburator;
        Part* manifold;

        Car(CarModel model);
        ~Car();

        // Part getters
        Part* GetGearbox() const { return gearbox; }
        Part* GetTires() const { return tires; }
        Part* GetEngine() const { return engine; }
        Part* GetCarburator() const { return carburator; }
        Part* GetManifold() const { return manifold; }

        CarModel GetModel();
        CarWheelType GetTireType();
        CarDecal GetDecal();
        int GetDriver();
};