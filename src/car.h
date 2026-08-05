#pragma once

#include "global.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "part.h"
#include "engine_part.h"
#include "car_system.h"
#include "race/race_sim.h"
#include <string>

class AnimatedBitmap;
//extern SDL_Renderer *renderer;

// CarWheelType now lives in car_system.h (part.h needs it and can't include car.h).
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
        CarModel model;
        SrLibCarDecal decal;
        std::string customDecalPath;
        CarScoopType scoop;
        StandardCarColor color;
        int tuneLevel = 9;

        Part gearbox;
        Part tires;
        EnginePart engine;
        Part carburator;
        Part manifold;

        // Top speed in mph, recomputed whenever a part changes.
        int topSpeed;

    public:

        Car();
        Car(CarModel model);
        ~Car();

        bool IsRoofChopped() const { return isRoofChopped; }
        void SetRoofChopped(bool chopped) { isRoofChopped = chopped; RecalculatePerformance(); }
        bool IsBumpersStripped() const { return isFrontBumperStripped && isBackBumperStripped; }
        bool IsFrontBumperStripped() const { return isFrontBumperStripped; }
        bool IsBackBumperStripped() const { return isBackBumperStripped; }
        void SetBumpersStripped(bool stripped) { isFrontBumperStripped = stripped; isBackBumperStripped = stripped; RecalculatePerformance(); }

        Part GetGearbox() const { return gearbox; }
        Part GetTires() const { return tires; }
        CarWheelType GetWheelType() const;
        EnginePart GetEngine() const {
            return engine;
        }
        Part GetCarburator() const { return carburator; }
        Part GetManifold() const { return manifold; }
        int GetTuneLevel() const { return tuneLevel; }
        CarModel GetModel();
        const CarModel& GetModelRef() const { return model; }
        SrLibCarDecal GetDecal();
        CarScoopType GetScoop() const { return scoop; }
        StandardCarColor GetColor() const { return color; }

        // Top speed in mph, the number the original prints as "Max speed".
        int GetTopSpeed() const { return topSpeed; }

        // Modification methods with validation
        bool CanChopRoof() const { return model.constraints.canChopRoof; }
        bool CanStripBumpers() const { return model.constraints.canStripBumpers; }
        bool CanAddDecal() const { return model.constraints.canAddDecal; }

        // Scoop is automatic with V8 engines
        bool HasScoop() const { return engine.HasScoop(); }

        void SetGearbox(Part gearbox) { this->gearbox = gearbox; RecalculatePerformance(); }
        void SetTires(CarWheelType type);
        void SetEngine(EnginePart engine) { this->engine = engine; RecalculatePerformance(); }
        void SetEngine(Part engine) { this->engine = EnginePart::FromPart(engine); RecalculatePerformance(); }
        void SetCarburator(Part carburator) { this->carburator = carburator; RecalculatePerformance(); }
        void SetManifold(Part manifold) { this->manifold = manifold; RecalculatePerformance(); }
        void SetDecal(SrLibCarDecal decal) { this->decal = decal; }
        const std::string& GetCustomDecalPath() const { return customDecalPath; }
        void SetCustomDecalPath(const std::string& path) { customDecalPath = path; }
        void SetColor(StandardCarColor color) { this->color = color; }
        void SetScoop(CarScoopType type);
        void SetTuneLevel(const int level) { this->tuneLevel = level; RecalculatePerformance(); }

    private:
        // Re-derives topSpeed from the installed parts (ComputeTopSpeed).
        void RecalculatePerformance();
};

// Car -> race-sim part classes (the packed class fields the original reads out
// of its parts table). weightClass is left at its default: the SR_data car-spec
// class byte is not reachable from a Car, only from an SrCarSpecEntry.
srrace::CarSetup CarSetupFromCar(const Car& car, bool automaticOverride);
