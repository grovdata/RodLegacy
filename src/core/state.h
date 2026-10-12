/*
 * RodLegacy - Global game state: State owns the scenes, cars, money, reputation, racer pool and the clock,
 * and moves between scenes.
 *
 * Created: 2024-01-26
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include "core/global.h"
#include "parsers/libp.h"
#include "core/assets.h"
#include "race/roster.h"
#include "car/cardata.h"
#include "car/car.h"
#include "race/reputation.h"
#include "race/racer_pool.h"
#include "core/game_clock.h"
#include "race/bet_acceptance.h"

#include <algorithm>
#include <set>
#include <vector>
#include <string>
#include <SDL.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

class Scene;
class Diner;
class Driving;
class Garage;
class KingOfCounty;
class Newspaper;
class GasStation;
class Racing;
class NameEntry;
class SaveLoadScene;
class GameOverMenu;
class Ending;
class Intro;
class Sr2Ending;
class GrudgeNight;
class CarDebug;
class SfxDebug;

class State {
private:
    int bankroll;
    bool debugMode;
    std::string playerName = "Player";
    bool jailed = false;

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    Reputation reputation;
    RacerPool  racerPool;
    // Every driver in play, from every enabled set that brings drivers, in one table.
    std::vector<SrDriverData>   srDriverData;
    std::vector<std::string>    srDriverNames;
    std::vector<int>            srDriverSets;   // parallel; indexes GetCarSetDefs()
    std::vector<SrCarSpecEntry> srCarSpecData;

    // Every enabled set's specs, Amiga set first so its entries stay at 0..25 as .SAV files store them.
    std::vector<SrCarSpecEntry> mergedCarSpecs;
    // Pool key per mergedCarSpecs entry; -1 for the King's slot in each set.
    std::vector<int>            mergedCarSpecModelIndex;
    int lastChallengerPoolIndex = -1;
    // His road race is forced next. Cleared by a loss to him in either race.
    bool kingQualifierWon = false;
    // Set when the phone list is opened while The King is available; cuts his arrival chance
    // from 50 % to 10 %. Cleared by a loss to him.
    bool kingListed = false;

    RaceType pendingRaceType   = RaceType::Road;
    int      pendingBetAmount  = 50;
    bool     pendingIsPinkSlip = false;

    std::vector<CarPtr> carRoster;
    std::vector<CarPtr> ownedCars;
    std::vector<CarPtr> classifiedCars;
    // The newspaper keeps an index-aligned copy of the ads and watches this.
    uint32_t classifiedsGeneration = 0;
    CarPtr currentCar;

    // Not attached to a car; one collection per category.
    std::vector<Part> spareTires;
    std::vector<Part> spareTransmissions;
    std::vector<Part> spareManifolds;
    std::vector<Part> spareCarburetors;
    std::vector<EnginePart> spareEngines;
    std::vector<Part> spareExhausts;
    std::vector<Part> spareMufflers;
    std::vector<Part> spareDifferentials;

    Roster roster;

    SrLibGameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;

    // 3600 ticks/day, 0 = June 16, 1963 (midnight).
    uint32_t elapsedTicks = 0;

    // Chosen once per profile and persisted; not re-rolled per race.
    int trackVariant = 0;

    // Seed for the roadside scatter.
    uint32_t scatterSeed = 0;

    AssetManager assets;

    std::unique_ptr<Garage> garage;
    std::unique_ptr<Diner> diner;
    std::unique_ptr<GasStation> gasStation;
    std::unique_ptr<Driving> driving;
    std::unique_ptr<Racing> racing;
    std::unique_ptr<Newspaper> newspaper;
    std::unique_ptr<KingOfCounty> kingOfCounty;
    std::unique_ptr<NameEntry> nameEntry;
    std::unique_ptr<SaveLoadScene> saveLoad;
    std::unique_ptr<GameOverMenu> gameOverMenu;
    std::unique_ptr<Ending> ending;
    std::unique_ptr<Sr2Ending> sr2Ending;
    std::unique_ptr<Intro> intro;
    std::unique_ptr<GrudgeNight> grudgeNight;
    bool pendingCruise = false;
    SrLibGameScene pendingCruiseDestination = SCENE_DINER;
    bool arrivingFromCruise = false;
#ifdef RODLEGACY_DEBUG_UI
    std::unique_ptr<CarDebug> carDebug;
    std::unique_ptr<SfxDebug> sfxDebug;
#endif


public:
    State();
    ~State();

    struct HallEntry { std::string name; int score; };
    std::vector<HallEntry> LoadHallDat() const;
    void SaveHallDat(const std::vector<HallEntry>& entries) const;
    std::string GetPlayerName() const { return playerName; }
    void SetPlayerName(const std::string& name) { playerName = name.empty() ? "Player" : name; }

    AssetManager& GetAssets() { return assets; }
    Newspaper* GetNewspaper() const { return newspaper.get(); }
    SaveLoadScene* GetSaveLoadScene() const { return saveLoad.get(); }
    Driving* GetDrivingScene() const { return driving.get(); }

    // MoveToScene turns a move between locations into SCENE_CRUISING and parks the destination here.
    // ArriveFromCruise marks the arrival so it is not charged or driven again.
    bool TakePendingCruise(SrLibGameScene& destination);
    void ArriveFromCruise(SrLibGameScene destination);

    const std::vector<SrCarSpecEntry>& GetMergedCarSpecs() const { return mergedCarSpecs; }
    const std::vector<int>& GetCarSpecModelIndex() const { return mergedCarSpecModelIndex; }

    void Init(bool debugMode, const fs::path& gamePath, bool muteAudio = false,
              SrGame game = SrGame::SR1);
    void InitWithCustomPath(bool debugMode, const std::string& customPath);

    int GetBankroll();
    void SetBankroll(int amount);

    SrLibGameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(SrLibGameScene scene);

    // Leaves the player carless, as a fresh Amiga boot.
    void StartNewGame();
    // Call after changing which sets are on.
    void RebuildCarSets();
    void GenerateClassifieds();

    // No car, no spares, and less than the cheapest car in the paper.
    bool IsOutOfDough() const;

    // Fined more than you had: the diner seen from a cell.
    bool IsJailed() const { return jailed; }
    void SetJailed(bool v) { jailed = v; }
    // For the debug UI checkbox (Diner::GetDebugPanels), so the jail view can
    // be previewed without losing a race to a cop first.
    bool* DebugJailedFlag() { return &jailed; }

    Opponent* GetNextOpponent();
    Opponent* GetCurrentOpponent();
    bool IsKingOpponent(const Opponent* opp) const { return roster.IsKing(opp); }
    // Null if his car spec is missing.
    Opponent* GetKingOpponent();

    void AddKnownOpponent(Opponent* opp);
    const std::vector<Opponent*>& GetKnownOpponents() const;
    void SetNextOpponent(Opponent* opp);

    void SetPendingRace(RaceType type, int betAmount, bool pinkSlip) {
        pendingRaceType   = type;
        pendingBetAmount  = betAmount;
        pendingIsPinkSlip = pinkSlip;
    }
    RaceType GetPendingRaceType()   const { return pendingRaceType; }
    int      GetPendingBetAmount()  const { return pendingBetAmount; }
    bool     GetPendingIsPinkSlip() const { return pendingIsPinkSlip; }

    void PickNextOpponent();

    // Null for the King's slot.
    CarPtr BuildCarForSpec(int specIndex);
    Opponent* GiveOpponentPoolCar(const RacerEntry& entry);
    void GiveOpponentsPoolCars();

    // Percent chance that the next arrival at the diner is The King.
    int KingArrivalChance() const;
    // Before the player has any rep (tier 1) the King only drives past the diner (DinerCar, SR1 0x23d5ca).
    bool KingOnlyDrivesBy() const;
    void OnPhoneListOpened();

    // Call for every finished race. settlesBet is false for a ticket or a jumped light, which still
    // count the race. Returns true if the King was beaten in his road race (end-game).
    bool OnRaceFinished(RaceType raceType, bool won, bool settlesBet);

    bool IsKingQualifierWon() const { return kingQualifierWon; }

    Reputation& GetReputation() { return reputation; }
    // nullptr without a pool-backed challenger.
    const SrDriverData* GetCurrentDriverData();
    int GetLastChallengerPoolIndex() const { return lastChallengerPoolIndex; }
    RacerPool&  GetRacerPool()  { return racerPool; }

    int GetPlayerScore()     const;
    int GetChallengerScore() const;

    // SR_data car spec of the challenger the pool last selected (The King's
    // included), or nullptr when there is no pool-backed challenger.
    const SrCarSpecEntry* GetChallengerSpec() const;
    BetContext GetBetContext();

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();

    int  GetGameHours() const { return GameClock::TicksToHours(elapsedTicks); }
    void AdvanceGameHours(int hours) { elapsedTicks += GameClock::HoursToTicks(hours); }
    // The Amiga games' activity costs, 150 ticks an hour; both games charge the same (docs/sr2_amiga/clock.md).
    void AdvanceTicks(uint32_t ticks) { elapsedTicks += ticks; }

    uint32_t GetElapsedTicks() const { return elapsedTicks; }
    void SetElapsedTicks(uint32_t ticks) { elapsedTicks = ticks; }

    uint32_t GetScatterSeed() const { return scatterSeed; }
    int  GetTrackVariant() const { return trackVariant; }
    void SetTrackVariant(int v) { trackVariant = ((v % 10) + 10) % 10; }

    std::string GetGameDateString() const {
        return GameClock::FormatDate(elapsedTicks, assets.GetGame() == SrGame::SR2 ? 1969 : 1963);
    }
    std::string GetGameClockString() const;
    int  CalculateScore() const;

    Car* GetCurrentCar();
    // 2 tenths per drive leg; 5 (drag) / 15 (road) per race.
    static constexpr int FUEL_PER_TRIP_TENTHS = 2;
    static constexpr int FUEL_PER_DRAG_RACE_TENTHS = 5;
    static constexpr int FUEL_PER_ROAD_RACE_TENTHS = 15;
    // Street Rod 2's Aqueduct.
    static constexpr int FUEL_PER_AQUEDUCT_RACE_TENTHS = 10;
    void BurnFuel(int tenths);
    const CarPtr& GetCurrentCarPtr() const { return currentCar; }
    void SetCurrentCar(CarPtr car);
    const std::vector<CarPtr>& GetOwnedCars() const { return ownedCars; }
    void AddOwnedCar(CarPtr car);
    // Does not touch the current car; a caller selling the car it drives clears that.
    void RemoveOwnedCar(const CarPtr& car);
    // Falls back to another car in the garage, or none.
    void ScrapCurrentCar();
    int GetCarValue(const Car* car);
    int GetCarStrippedValue(const Car* car);

    // False, with state unchanged, on a malformed file.
    bool LoadSaveGame(const fs::path& path);

    // Round-trips everything State holds. Both return false and leave State untouched on a bad file.
    bool SaveGameToml(const fs::path& path);
    bool LoadGameToml(const fs::path& path);

    const std::vector<Part>& GetSpareTires() const { return spareTires; }
    void AddSpareTire(Part tire) { spareTires.push_back(tire); }
    bool RemoveSpareTire(CarWheelType type) {
        auto it = std::find_if(spareTires.begin(), spareTires.end(),
                                [type](const Part& p) { return p.partType.tireType == type; });
        if (it == spareTires.end()) return false;
        spareTires.erase(it);
        return true;
    }

    const std::vector<Part>& GetSpareTransmissions() const { return spareTransmissions; }
    void AddSpareTransmission(Part p) { spareTransmissions.push_back(p); }
    bool RemoveSpareTransmission(TransmissionType type) {
        auto it = std::find_if(spareTransmissions.begin(), spareTransmissions.end(),
                                [type](const Part& p) { return p.partType.transmissionType == type; });
        if (it == spareTransmissions.end()) return false;
        spareTransmissions.erase(it);
        return true;
    }

    const std::vector<Part>& GetSpareManifolds() const { return spareManifolds; }
    void AddSpareManifold(Part p) { spareManifolds.push_back(p); }
    bool RemoveSpareManifold(ManifoldType type) {
        auto it = std::find_if(spareManifolds.begin(), spareManifolds.end(),
                                [type](const Part& p) { return p.partType.manifoldType == type; });
        if (it == spareManifolds.end()) return false;
        spareManifolds.erase(it);
        return true;
    }

    const std::vector<Part>& GetSpareCarburetors() const { return spareCarburetors; }
    void AddSpareCarburetor(Part p) { spareCarburetors.push_back(p); }
    bool RemoveSpareCarburetor(CarburetorType type) {
        auto it = std::find_if(spareCarburetors.begin(), spareCarburetors.end(),
                                [type](const Part& p) { return p.partType.carburetorType == type; });
        if (it == spareCarburetors.end()) return false;
        spareCarburetors.erase(it);
        return true;
    }

    const std::vector<EnginePart>& GetSpareEngines() const { return spareEngines; }
    void AddSpareEngine(EnginePart p) { spareEngines.push_back(p); }
    bool RemoveSpareEngine(CarEngine_t type) {
        auto it = std::find_if(spareEngines.begin(), spareEngines.end(),
                                [type](const EnginePart& p) { return p.engineType == type; });
        if (it == spareEngines.end()) return false;
        spareEngines.erase(it);
        return true;
    }

    // Street Rod 2's three under-car categories.
    std::vector<Part>& GetSpareSr2(PartCategory category) {
        if (category == PartCategory::Muffler) return spareMufflers;
        if (category == PartCategory::Differential) return spareDifferentials;
        return spareExhausts;
    }
    const std::vector<Part>& GetSpareSr2(PartCategory category) const {
        return const_cast<State*>(this)->GetSpareSr2(category);
    }

    bool RemoveSpareAt(PartCategory category, size_t index) {
        auto eraseAt = [index](auto& v) {
            if (index >= v.size()) return false;
            v.erase(v.begin() + static_cast<long>(index));
            return true;
        };
        switch (category) {
            case PartCategory::Engine:       return eraseAt(spareEngines);
            case PartCategory::Transmission: return eraseAt(spareTransmissions);
            case PartCategory::Manifold:     return eraseAt(spareManifolds);
            case PartCategory::Carburetor:   return eraseAt(spareCarburetors);
            case PartCategory::Tire:         return eraseAt(spareTires);
            case PartCategory::ExhaustManifold:
            case PartCategory::Muffler:
            case PartCategory::Differential: return eraseAt(GetSpareSr2(category));
        }
        return false;
    }

    Roster& GetRoster() { return roster; }
    GrudgeNight* GetGrudgeNight() { return grudgeNight.get(); }
    std::vector<CarPtr>& GetClassifiedCars() { return classifiedCars; }
    uint32_t GetClassifiedsGeneration() const { return classifiedsGeneration; }
    CarData carData;

};

extern State state;
