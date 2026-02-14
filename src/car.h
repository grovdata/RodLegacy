#pragma once

#include "global.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "part.h"
#include "engine_part.h"
#include "car_system.h"
#include <string>

class Spritesheet;
//extern SDL_Renderer *renderer;

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

        // Simple performance tracking
        int effectiveSpeed;
        int effectiveAcceleration;

    public:

        Car();
        Car(CarModel model);
        ~Car();

        bool IsRoofChopped() const { return isRoofChopped; }
        void SetRoofChopped(bool chopped) { isRoofChopped = chopped; }
        bool IsBumpersStripped() const { return isFrontBumperStripped && isBackBumperStripped; }
        void SetBumpersStripped(bool stripped) { isFrontBumperStripped = stripped; isBackBumperStripped = stripped; }

        Part GetGearbox() const { return gearbox; }
        Part GetTires() const { return tires; }
        EnginePart GetEngine() const {
            return engine;
        }
        Part GetCarburator() const { return carburator; }
        Part GetManifold() const { return manifold; }
        int GetTuneLevel() const { return tuneLevel; }
        CarModel GetModel();
        SrLibCarDecal GetDecal();
        CarScoopType GetScoop() const { return scoop; }
        StandardCarColor GetColor() const { return color; }

        // Simple performance methods
        int GetEffectiveSpeed() const { return effectiveSpeed; }
        int GetEffectiveAcceleration() const { return effectiveAcceleration; }

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
        void SetTuneLevel(const int level) { this->tuneLevel = level; }

    private:
        // Formula-based performance calculation
        void RecalculatePerformance();

        // Helper methods for part speed bonuses
        int GetTransmissionSpeedBonus(TransmissionType type, int wear) const;
        int GetManifoldSpeedBonus(ManifoldType type, int wear) const;
        int GetCarburetorSpeedBonus(CarburetorType type, int wear) const;
};
