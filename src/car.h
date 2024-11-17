#pragma once

#include "global.h"
#include "parsers/libp.h"
#include "part.h"

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
        CarModel model;
        CarDecal decal;

        Part* gearbox;
        Part* tires;
        Part* engine;
        Part* carburator;
        Part* manifold;

    public:

        Car();
        Car(CarModel model);
        ~Car();

        // Part getters
        Part* GetGearbox() const { return gearbox; }
        Part* GetTires() const { return tires; }
        Part* GetEngine() const { return engine; }
        Part* GetCarburator() const { return carburator; }
        Part* GetManifold() const { return manifold; }

        void SetGearbox(Part* gearbox) { this->gearbox = gearbox; }
        void SetTires(Part* tires) { this->tires = tires; }
        void SetEngine(Part* engine) { this->engine = engine; }
        void SetCarburator(Part* carburator) { this->carburator = carburator; }
        void SetManifold(Part* manifold) { this->manifold = manifold; }

        CarModel GetModel();

        CarDecal GetDecal();
        int GetDriver();
};