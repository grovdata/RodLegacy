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

        // Gallons x10 (matches the save format's own units - see
        // save_loader.h's fuelTenths). 160 = 16gal, a fresh car's full tank.
        int fuelTenths = 160;

        // SR_data car-spec class byte 0..8, the weight term in the top-speed
        // score. 4 until someone who knows the spec sets it.
        int weightClass = 4;

        // Engine/manifold scoring classes straight off the car's SR_data spec.
        // RodLegacy's own EnginePart/ManifoldType enums are coarser than the
        // original's fields (there is no ManifoldType for spec class 1, which
        // half the shipped cars use), so a car that came from a spec keeps its
        // real classes here. -1 = derive them from the installed parts, which
        // is what happens as soon as the player fits a different one.
        int specEngineClass = -1;
        int specManifoldClass = -1;

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
        int GetFuelTenths() const { return fuelTenths; }
        void SetFuelTenths(int tenths) { fuelTenths = tenths; }
        int GetTuneLevel() const { return tuneLevel; }
        CarModel GetModel();
        const CarModel& GetModelRef() const { return model; }
        SrLibCarDecal GetDecal();
        CarScoopType GetScoop() const { return scoop; }
        StandardCarColor GetColor() const { return color; }

        // Top speed in mph, the number the original prints as "Max speed".
        int GetTopSpeed() const { return topSpeed; }

        int GetWeightClass() const { return weightClass; }
        void SetWeightClass(int cls) { weightClass = cls; RecalculatePerformance(); }

        int GetSpecEngineClass() const { return specEngineClass; }
        int GetSpecManifoldClass() const { return specManifoldClass; }
        void SetSpecPartClasses(int engineCls, int manifoldCls) {
            specEngineClass = engineCls;
            specManifoldClass = manifoldCls;
            RecalculatePerformance();
        }

        // Modification methods with validation
        bool CanChopRoof() const { return model.constraints.canChopRoof; }
        bool CanStripBumpers() const { return model.constraints.canStripBumpers; }
        bool CanAddDecal() const { return model.constraints.canAddDecal; }

        // Scoop is automatic with V8 engines
        bool HasScoop() const { return engine.HasScoop(); }

        void SetGearbox(Part gearbox) { this->gearbox = gearbox; RecalculatePerformance(); }
        void SetTires(CarWheelType type) { SetTires(type, 0, 0); }
        void SetTires(CarWheelType type, int wear, int cost);
        // Fitting a part supersedes whatever the car's spec said about it.
        void SetEngine(EnginePart engine) { this->engine = engine; specEngineClass = -1; RecalculatePerformance(); }
        void SetEngine(Part engine) { SetEngine(EnginePart::FromPart(engine)); }
        void SetCarburator(Part carburator) { this->carburator = carburator; RecalculatePerformance(); }
        void SetManifold(Part manifold) { this->manifold = manifold; specManifoldClass = -1; RecalculatePerformance(); }
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
