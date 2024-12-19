#pragma once

#include "global.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "part.h"

class Spritesheet;
extern SDL_Renderer *renderer;

enum CarWheelType { STOCK, BRAND, RACING };
enum CarScoopType { SCOOP1, SCOOP2, SCOOP3, SCOOP4, SCOOP_NONE };

class Car {
    /*
        Car positions
         Garage: 42, 82
         Diner :
         Gas station:
    */
    private:
        bool isFrontBumperStripped;
        bool isBackBumperStripped;
        bool isRoofChopped;
        bool isMoving;
        int tuning;
        CarModel model;
        SrLibCarDecal decal;
        CarScoopType scoop;
        StandardCarColor color;

        Part* gearbox;
        Part* tires;
        Part* engine;
        Part* carburator;
        Part* manifold;

    public:

        Car();
        Car(CarModel model);
        ~Car();

        bool IsRoofChopped() const { return isRoofChopped; }
        void SetRoofChopped(bool chopped) { isRoofChopped = chopped; }
        bool IsBumpersStripped() const { return isFrontBumperStripped && isBackBumperStripped; }
        void SetBumpersStripped(bool stripped) { isFrontBumperStripped = stripped; isBackBumperStripped = stripped; }

        // Part getters
        Part* GetGearbox() const { return gearbox; }
        Part* GetTires() const { return tires; }
        Part* GetEngine() const { return engine; }
        Part* GetCarburator() const { return carburator; }
        Part* GetManifold() const { return manifold; }

        void SetGearbox(Part* gearbox) { this->gearbox = gearbox; }
        void SetTires(CarWheelType type);
        void SetEngine(Part* engine) { this->engine = engine; }
        void SetCarburator(Part* carburator) { this->carburator = carburator; }
        void SetManifold(Part* manifold) { this->manifold = manifold; }
        void SetDecal(SrLibCarDecal decal) { this->decal = decal; }
        void SetColor(StandardCarColor color) { this->color = color; }
        void SetScoop(CarScoopType type);
        CarModel GetModel();
        SrLibCarDecal GetDecal();
        CarScoopType GetScoop() const { return scoop; }
        StandardCarColor GetColor() const { return color; }
};