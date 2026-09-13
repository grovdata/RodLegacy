#pragma once

#include "core/global.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "car/part.h"
#include "car/engine_part.h"
#include "car/car_system.h"
#include "race/race_sim.h"
#include <memory>
#include <string>

class AnimatedBitmap;
//extern SDL_Renderer *renderer;

// CarWheelType now lives in car_system.h (part.h needs it and can't include car.h).

// Cars are shared-owned: State holds the player's roster and the classifieds,
// but the Garage caches a rendered CarBitmap per car and scenes hold onto the
// car being animated, all of which outlive any single call. Loading a save
// replaces State's whole roster, so a raw pointer held elsewhere would dangle
// - sharing ownership is what makes that safe rather than a rule everyone has
// to remember. Pass a plain Car* or const Car& to anything that only reads a
// car for the duration of a call; CarPtr is for storage.
class Car;
using CarPtr = std::shared_ptr<Car>;

// Which lib.p bitmap represents each tire type. Shared by Car::SetTires and
// by everything that builds a loose (not-yet-fitted) tire Part - the
// newspaper's parts counter and save loading.
AmigaLibAsset TireAssetForType(CarWheelType type);

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
        // Initialised here: a car built by CarBuilder (save loading) has no
        // colour set until its caller supplies one, and an unset colour used
        // to reach GetStandardCarColor as a garbage value.
        StandardCarColor color = CarColorRed;
        int tuneLevel = kPerfectTuneLevel;

        Part gearbox;
        Part tires;
        EnginePart engine;
        Part manifold;
        // The original carries three separate carburetor pointers on the car
        // struct (+0x0A slots 2-4) and the manifold class decides how many
        // are used - a dual quad runs two, a triple 2-barrel three. Each is
        // fitted and removed on its own, so each needs its own "is it there".
        Part carburetors[kMaxCarburetors];
        bool carbFitted[kMaxCarburetors] = {false, false, false};
        // A part pulled in the engine bay really leaves the car, so the car
        // stops running until it is replaced - see IsRunning(). The gearbox
        // only goes missing by dropping it in a race; the transmission
        // window puts its own back if you change your mind.
        bool engineFitted = true;
        bool manifoldFitted = true;
        bool gearboxFitted = true;

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

        // The car's worth with nothing bolted on - see SetValue/GetValue.
        int bodyValue = 0;

        // Sub-percent race wear not yet rolled into the parts' own wear.
        // Deliberately not saved: it is worth less than 1%.
        int engineWearRemainder = 0;
        int gearboxWearRemainder = 0;
        int tireWearRemainder = 0;

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

        // A car loaded from its SR_data spec never had a concrete part
        // fitted for engine/manifold, so those resolve from the spec class
        // instead of reading a default-constructed Part's garbage.
        CarEngine_t GetEngineType() const;
        ManifoldType GetManifoldType() const;

        // How many carburetors this car's manifold mounts, and which of
        // those mounts currently hold one.
        int GetCarburetorSlots() const { return CarburetorsPerManifold(GetManifoldType()); }
        bool HasCarburetor(int slot) const { return slot >= 0 && slot < kMaxCarburetors && carbFitted[slot]; }
        Part GetCarburetor(int slot) const { return HasCarburetor(slot) ? carburetors[slot] : Part{}; }
        // The first fitted carb - every mount on a car carries the same type,
        // so this is the car's carburetor for scoring and display.
        Part GetCarburator() const;
        int FittedCarburetorCount() const;

        bool HasEngine() const { return engineFitted; }
        bool HasGearbox() const { return gearboxFitted; }
        bool HasManifold() const { return manifoldFitted; }
        // The original refuses to move a car whose engine is apart:
        // "Your car's not running, speedy!"
        bool IsRunning() const;
        int GetFuelTenths() const { return fuelTenths; }
        void SetFuelTenths(int tenths) { fuelTenths = tenths; }
        int GetTuneLevel() const { return tuneLevel; }
        CarModel GetModel();
        const CarModel& GetModelRef() const { return model; }
        SrLibCarDecal GetDecal();
        StandardCarColor GetColor() const { return color; }

        // Top speed in mph, the number the original prints as "Max speed".
        int GetTopSpeed() const { return topSpeed; }

        int GetWeightClass() const { return weightClass; }
        void SetWeightClass(int cls) { weightClass = cls; RecalculatePerformance(); }

        int GetSpecEngineClass() const { return specEngineClass; }
        // Engine tier 0..2 (V6 / small V8 / big V8): the spec's own class
        // while the car still carries its spec engine, the fitted part's tier
        // once one has been swapped in. Scoring and bet strength both use it.
        int GetEngineClass() const;
        int GetSpecManifoldClass() const { return specManifoldClass; }
        void SetSpecPartClasses(int engineCls, int manifoldCls) {
            specEngineClass = engineCls;
            specManifoldClass = manifoldCls;
            // The spec's manifold class is what decides the carb mounts.
            RefitCarburetorMounts();
        }

        // Modification methods with validation
        bool CanChopRoof() const { return model.constraints.canChopRoof; }
        bool CanStripBumpers() const { return model.constraints.canStripBumpers; }
        bool CanAddDecal() const { return model.constraints.canAddDecal; }

        // Scoop is automatic with V8 engines. Goes through GetEngineType() so
        // a car still on its SR_data spec answers from its spec class - the
        // `engine` member is CarBuilder's placeholder V6 until a part is
        // actually fitted.
        bool HasScoop() const { return EngineTypeHasScoop(GetEngineType()); }

        void SetGearbox(Part gearbox) { this->gearbox = gearbox; gearboxFitted = true; RecalculatePerformance(); }
        void SetTires(CarWheelType type) { SetTires(type, 0, PartCatalogPrice(type)); }
        void SetTires(CarWheelType type, int wear, int cost);
        // Fits a set that already exists (a spare off the parts box), keeping
        // its wear and its classified-ad name.
        void SetTires(const Part& tire) { tires = tire; RecalculatePerformance(); }
        // Fitting a part supersedes whatever the car's spec said about it.
        void SetEngine(EnginePart engine) { this->engine = engine; engineFitted = true; specEngineClass = -1; RecalculatePerformance(); }
        void SetEngine(Part engine) { SetEngine(EnginePart::FromPart(engine)); }
        void SetManifold(Part manifold) {
            this->manifold = manifold; manifoldFitted = true; specManifoldClass = -1;
            RefitCarburetorMounts();
        }
        // Fits the same carburetor in every mount the manifold has - how a
        // spec-built car and a save's slot-3 entry both arrive.
        void SetCarburator(Part carburator);
        void SetCarburetor(int slot, Part carburator);

        // Racing wears the engine and the gearbox. The sim counts in the
        // original's 0..10000 units while Part::wear is RodLegacy's 0..100
        // percent, so the sub-percent remainder is carried here rather than
        // truncated away every race - otherwise a drag race (~150 raw) would
        // round down to 1% and lose a third of the wear each time.
        void AddEngineWearRaw(int raw);
        void AddGearboxWearRaw(int raw);
        // Tires wear by the race rather than the tick, but through the same
        // remainder carry - see srrace::TireWearForRace.
        void AddTireWearRaw(int raw);

        // Pulling a part in the engine bay: the car loses it for real, and
        // IsRunning() goes false until something is put back. The part's own
        // data is left alone so the bay can still lay out the mounts around
        // the hole while the rest comes off.
        void RemoveEngine() { engineFitted = false; RecalculatePerformance(); }
        void RemoveManifold() { manifoldFitted = false; RecalculatePerformance(); }
        void RemoveCarburetor(int slot);
        // A blown engine takes the whole assembly with it - block, manifold
        // and every carburetor. Nothing is salvaged; see racing.cpp.
        void StripEngineAssembly();
        // Dropped in a race - same deal, the gearbox is simply gone.
        void RemoveGearbox() { gearboxFitted = false; RecalculatePerformance(); }
        void SetDecal(SrLibCarDecal decal) { this->decal = decal; }
        const std::string& GetCustomDecalPath() const { return customDecalPath; }
        void SetCustomDecalPath(const std::string& path) { customDecalPath = path; }
        void SetColor(StandardCarColor color) { this->color = color; }
        void SetTuneLevel(const int level) { this->tuneLevel = level; RecalculatePerformance(); }

        // --- Worth, in dollars ---
        // The original keeps one live value on the car record: it starts at the
        // SR_data list price when the car is created (GenerateOpponentCar
        // 0x23bd6a) and moves by a part's own price every time one is fitted or
        // pulled (FitPart 0x24086a / DetachPart 0x242b16). Rather than hook
        // every install site, RodLegacy keeps the part-free remainder and adds
        // what is bolted on right now - the same number for any sequence of
        // swaps. SetValue seeds it, so call it once the parts are on.
        void SetValue(int value) { bodyValue = value - GetPartsValue(); }
        int GetValue() const { return bodyValue + GetPartsValue(); }
        // Everything FitPart can put on a car: drivetrain, every fitted
        // carburetor, and the tires.
        int GetPartsValue() const;
        // What DismantleCar (0x242b4c) hands back: every slot comes off and is
        // deducted EXCEPT the tires, which it detaches without crediting. Never
        // negative - the original clamps it the same way, and on a stock car
        // the drivetrain is usually worth more than the shell.
        int GetStrippedValue() const;

    private:
        // Fills exactly the mounts this manifold has with the fitted
        // carburetor, and clears the rest.
        void RefitCarburetorMounts();
        // Re-derives topSpeed from the installed parts (ComputeTopSpeed).
        void RecalculatePerformance();
};

// Car -> race-sim part classes (the packed class fields the original reads out
// of its parts table). weightClass is left at its default: the SR_data car-spec
// class byte is not reachable from a Car, only from an SrCarSpecEntry.
srrace::CarSetup CarSetupFromCar(const Car& car, bool automaticOverride);
