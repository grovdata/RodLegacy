#pragma once
#include "global.h"
#include "parsers/libp.h"  // For SrLibGameScene
#include "assets.h"
#include "roster.h"
#include "cardata.h"
#include "car.h"
#include "reputation.h"
#include "racer_pool.h"
#include "game_clock.h"

#include <algorithm>
#include <set>
#include <vector>
#include <string>
#include <SDL.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Forward declarations
class Scene;
class Diner;
class Driving;
class Garage;
class KingOfCounty;
class Newspaper;
class GasStation;
class Racing;
class NameEntry;

class State {
private:
    int bankroll;
    // int dragRacesCompleted;
    // int roadRacesCompleted;
    bool debugMode;
    std::string playerName = "Player";

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    Reputation reputation;
    RacerPool  racerPool;
    std::vector<SrDriverData>   srDriverData;
    std::vector<SrCarSpecEntry> srCarSpecData;
    int lastChallengerPoolIndex = -1;

    RaceType pendingRaceType   = RaceType::Road;
    int      pendingBetAmount  = 50;
    bool     pendingIsPinkSlip = false;

    std::vector<Car *> carRoster;
    std::vector<Car *> ownedCars;
    std::vector<Car *> classifiedCars; // Cars for sale in the newspaper
    Car *currentCar = nullptr;

    // Spare-parts inventory (segment 4 of the save format -- not attached to
    // a car), one collection per category, mirroring Car's own per-category
    // slots so both installed and spare parts use the same Part/EnginePart
    // representation.
    std::vector<Part> spareTires;
    std::vector<Part> spareTransmissions;
    std::vector<Part> spareManifolds;
    std::vector<Part> spareCarburetors;
    std::vector<EnginePart> spareEngines;

    Roster roster;

    SrLibGameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;

    // Raw elapsed-tick counter, matching the Amiga save format's trailer
    // chunk 7 exactly (RodLegacyResearch/docs/formats/save_game.md): 3600
    // ticks/day, 0 = June 16, 1963 (midnight). Confirmed live and from the
    // game's own VBlank-driven increment code this session.
    uint32_t elapsedTicks = 0;

    // AssetManager first so it is destroyed last; scenes hold non-owning refs to its assets
    AssetManager assets;

    std::unique_ptr<Garage> garage;
    std::unique_ptr<Diner> diner;
    std::unique_ptr<GasStation> gasStation;
    std::unique_ptr<Driving> driving;
    std::unique_ptr<Racing> racing;
    std::unique_ptr<Newspaper> newspaper;
    std::unique_ptr<KingOfCounty> kingOfCounty;
    std::unique_ptr<NameEntry> nameEntry;

    // Remove this later
    const std::string GetTomlFile();

public:
    State();
    ~State();

    struct HallEntry { std::string name; int score; };
    std::vector<HallEntry> LoadHallDat() const;
    void SaveHallDat(const std::vector<HallEntry>& entries) const;
    std::string GetPlayerName() const { return playerName; }
    void SetPlayerName(const std::string& name) { playerName = name.empty() ? "Player" : name; }

    AssetManager& GetAssets() { return assets; }

    void Init(bool debugMode, const fs::path& gamePath, bool muteAudio = false);
    void InitWithCustomPath(bool debugMode, const std::string& customPath);

    int GetBankroll();
    void SetBankroll(int amount);

    SrLibGameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(SrLibGameScene scene);

    Opponent* GetNextOpponent();
    Opponent* GetCurrentOpponent();

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

    // Pool-based opponent selection. Picks from RacerPool (or The King), updates
    // lastChallengerPoolIndex, and wires the result into the Roster so existing
    // Diner code continues to work unchanged via GetNextOpponent().
    void PickNextOpponent();

    // Call after a race finishes. won=true means the player crossed first.
    // Whether the stake was a pink slip is taken from the pending bet.
    // Updates Reputation, RacerPool, and bankroll as appropriate.
    // Returns true if the King was beaten in a drag race (triggers end-game).
    bool OnRaceFinished(RaceType raceType, bool won);

    Reputation& GetReputation() { return reputation; }
    // SR_data driver row (tier / reaction / aggression) for the challenger the
    // pool last selected, or nullptr when there is no pool-backed challenger.
    const SrDriverData* GetCurrentDriverData();
    int GetLastChallengerPoolIndex() const { return lastChallengerPoolIndex; }
    RacerPool&  GetRacerPool()  { return racerPool; }

    // Performance scores for bet-acceptance logic.
    int GetPlayerScore()     const;
    int GetChallengerScore() const;

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();

    // Hour-granularity convenience wrappers over elapsedTicks (kept for
    // existing call sites); prefer GetElapsedTicks()/SetElapsedTicks() for
    // anything that needs to round-trip exactly through a save file. Actual
    // date/clock math lives in game_clock.h, independent of State, so it
    // can be unit tested without constructing a State.
    int  GetGameHours() const { return GameClock::TicksToHours(elapsedTicks); }
    void AdvanceGameHours(int hours) { elapsedTicks += GameClock::HoursToTicks(hours); }

    uint32_t GetElapsedTicks() const { return elapsedTicks; }
    void SetElapsedTicks(uint32_t ticks) { elapsedTicks = ticks; }

    std::string GetGameDateString() const { return GameClock::FormatDate(elapsedTicks); }
    std::string GetGameClockString() const;
    int  CalculateScore() const;

    Car* GetCurrentCar();
    void SetCurrentCar(Car *car);
    std::vector<Car *> GetOwnedCars();
    void AddOwnedCar(Car* car);

    // Loads a real Amiga .sav file, replacing bankroll, owned cars, known-
    // opponent car specs, race record, and elapsed time. Returns false
    // (state left unchanged) on a malformed file. See save_loader.h for
    // the parser and its documented gaps; RodLegacyResearch/docs/formats/
    // save_game.md is the authoritative format reference.
    bool LoadSaveGame(const fs::path& path);

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

    const std::vector<Part>& GetSpareManifolds() const { return spareManifolds; }
    void AddSpareManifold(Part p) { spareManifolds.push_back(p); }

    const std::vector<Part>& GetSpareCarburetors() const { return spareCarburetors; }
    void AddSpareCarburetor(Part p) { spareCarburetors.push_back(p); }

    const std::vector<EnginePart>& GetSpareEngines() const { return spareEngines; }
    void AddSpareEngine(EnginePart p) { spareEngines.push_back(p); }

    Roster& GetRoster() { return roster; }
    std::vector<Car *>& GetClassifiedCars() { return classifiedCars; }
    CarData carData;

};

extern State state;
