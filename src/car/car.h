/*
 * RodLegacy - A player or opponent car: Car holds its parts, paint, mods, fuel, wear and value, and
 * CarSetupFromCar feeds the race sim.
 *
 * Created: 2024-01-06
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "car/part.h"
#include "car/engine_part.h"
#include "car/car_system.h"
#include "race/race_sim.h"
#include <algorithm>
#include <memory>
#include <string>

class AnimatedBitmap;

// Shared-owned: loading a save replaces State's roster while caches and scenes still hold cars.
// Pass Car* or const Car& for the duration of a call; CarPtr is for storage.
class Car;
using CarPtr = std::shared_ptr<Car>;

// Shared by Car::SetTires and anything building a loose tire Part.
AmigaLibAsset TireAssetForType(CarWheelType type);

class Car {
    private:
        bool isFrontBumperStripped;
        bool isBackBumperStripped;
        bool isRoofChopped;
        CarModel model;
        SrLibCarDecal decal;
        std::string customDecalPath;
        // Initialised: a CarBuilder car has no colour until its caller sets one.
        StandardCarColor color = CarColorRed;
        int tuneLevel = kPerfectTuneLevel;

        Part gearbox;
        Part tires;
        EnginePart engine;
        Part manifold;
        // Each carburetor mount is fitted and removed on its own.
        Part carburetors[kMaxCarburetors];
        bool carbFitted[kMaxCarburetors] = {false, false, false};
        // A pulled part really leaves the car. The gearbox only goes missing by dropping it in a race.
        bool engineFitted = true;
        bool manifoldFitted = true;
        bool gearboxFitted = true;

        // Gallons x10. 160 = 16 gal, a fresh car's full tank.
        int fuelTenths = 160;

        // The weight term in the top-speed score.
        int weightClass = 4;

        // Scoring classes from the SR_data spec, which are finer than RodLegacy's enums. -1 = derive from the parts.
        int specEngineClass = -1;
        int specManifoldClass = -1;

        int topSpeed;

        // Worth with nothing bolted on.
        int bodyValue = 0;

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
        Part GetManifold() const { return manifold; }

        // A spec-built car resolves engine/manifold from the spec class.
        CarEngine_t GetEngineType() const;
        ManifoldType GetManifoldType() const;

        int GetCarburetorSlots() const { return CarburetorsPerManifold(GetManifoldType()); }
        bool HasCarburetor(int slot) const { return slot >= 0 && slot < kMaxCarburetors && carbFitted[slot]; }
        Part GetCarburetor(int slot) const { return HasCarburetor(slot) ? carburetors[slot] : Part{}; }
        Part GetCarburator() const;
        int FittedCarburetorCount() const;

        bool HasEngine() const { return engineFitted; }
        bool HasGearbox() const { return gearboxFitted; }
        bool HasManifold() const { return manifoldFitted; }
        // "Your car's not running, speedy!"
        bool IsRunning() const;
        int GetFuelTenths() const { return fuelTenths; }
        void SetFuelTenths(int tenths) { fuelTenths = tenths; }
        int GetTuneLevel() const { return tuneLevel; }
        CarModel GetModel();
        const CarModel& GetModelRef() const { return model; }
        SrLibCarDecal GetDecal();
        StandardCarColor GetColor() const { return color; }

        int GetTopSpeed() const { return topSpeed; }

        int GetWeightClass() const { return weightClass; }
        void SetWeightClass(int cls) { weightClass = cls; RecalculatePerformance(); }

        int GetSpecEngineClass() const { return specEngineClass; }
        // Tier 0..2: the spec's class while the spec engine is in, the fitted part's tier after a swap.
        int GetEngineClass() const;
        int GetSpecManifoldClass() const { return specManifoldClass; }
        void SetSpecPartClasses(int engineCls, int manifoldCls) {
            specEngineClass = engineCls;
            specManifoldClass = manifoldCls;
            // The spec's manifold class is what decides the carb mounts.
            RefitCarburetorMounts();
        }

        bool CanChopRoof() const { return model.constraints.canChopRoof; }
        bool CanStripBumpers() const { return model.constraints.canStripBumpers; }
        bool CanAddDecal() const { return model.constraints.canAddDecal; }

        // Goes through GetEngineType() so a spec car answers from its class.
        bool HasScoop() const { return EngineTypeHasScoop(GetEngineType()); }

        void SetGearbox(Part gearbox) { this->gearbox = gearbox; gearboxFitted = true; RecalculatePerformance(); }
        void SetTires(CarWheelType type) { SetTires(type, 0, PartCatalogPrice(type)); }
        void SetTires(CarWheelType type, int wear, int cost);
        // Keeps the spare's wear and ad name.
        void SetTires(const Part& tire) { tires = tire; RecalculatePerformance(); }
        void SetEngine(EnginePart engine) { this->engine = engine; engineFitted = true; specEngineClass = -1; RecalculatePerformance(); }
        void SetEngine(Part engine) { SetEngine(EnginePart::FromPart(engine)); }
        void SetManifold(Part manifold) {
            this->manifold = manifold; manifoldFitted = true; specManifoldClass = -1;
            RefitCarburetorMounts();
        }
        // Fills every mount the manifold has.
        void SetCarburator(Part carburator);
        void SetCarburetor(int slot, Part carburator);

        // Carries the sub-percent remainder so wear isn't truncated away every race.
        void AddEngineWearRaw(int raw);
        void AddGearboxWearRaw(int raw);
        void AddTireWearRaw(int raw);

        // The part's data is kept so the bay can lay out the mounts around the hole.
        void RemoveEngine() { engineFitted = false; RecalculatePerformance(); }
        void RemoveManifold() { manifoldFitted = false; RecalculatePerformance(); }
        void RemoveCarburetor(int slot);
        // Block, manifold and every carburetor; nothing is salvaged.
        void StripEngineAssembly();
        void RemoveGearbox() { gearboxFitted = false; RecalculatePerformance(); }
        void SetDecal(SrLibCarDecal decal) { this->decal = decal; }
        const std::string& GetCustomDecalPath() const { return customDecalPath; }
        void SetCustomDecalPath(const std::string& path) { customDecalPath = path; }
        void SetColor(StandardCarColor color) { this->color = color; }
        void SetTuneLevel(const int level) { this->tuneLevel = level; RecalculatePerformance(); }

        // Keeps the part-free remainder and adds what is bolted on now, the same for any sequence of swaps.
        // Call once the parts are on.
        void SetValue(int value) { bodyValue = value - GetPartsValue(); }
        int GetValue() const { return bodyValue + GetPartsValue(); }
        int GetPartsValue() const;
        // Everything except the tires, which come off uncredited. Never negative.
        int GetStrippedValue() const;

    private:
        void RefitCarburetorMounts();
        void RecalculatePerformance();
};

// weightClass is left at its default: the spec class byte is not reachable from a Car.
srrace::CarSetup CarSetupFromCar(const Car& car, bool automaticOverride);
