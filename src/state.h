#pragma once
#include "global.h"
#include "parsers/libp.h"  // For SrLibGameScene
#include "assets.h"
#include "roster.h"
#include "cardata.h"
#include "car.h"
#include "reputation.h"
#include "racer_pool.h"

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

class State {
private:
    int bankroll;
    // int dragRacesCompleted;
    // int roadRacesCompleted;
    bool debugMode;

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
    Car *currentCar;
    std::vector<CarWheelType> spareTires;

    Roster roster;

    SrLibGameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;
    int gameHours = 12;  // game clock: hours since June 15 1963 midnight; starts at noon (12)

    // AssetManager first so it is destroyed last; scenes hold non-owning refs to its assets
    AssetManager assets;

    std::unique_ptr<Garage> garage;
    std::unique_ptr<Diner> diner;
    std::unique_ptr<GasStation> gasStation;
    std::unique_ptr<Driving> driving;
    std::unique_ptr<Racing> racing;
    std::unique_ptr<Newspaper> newspaper;
    std::unique_ptr<KingOfCounty> kingOfCounty;

    // Remove this later
    const std::string GetTomlFile();

public:
    State();
    ~State();

    struct HallEntry { std::string name; int score; };
    std::vector<HallEntry> LoadHallDat() const;
    void SaveHallDat(const std::vector<HallEntry>& entries) const;
    std::string GetPlayerName() const { return "Player"; }

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

    // Call after a race finishes.
    // won=true → player crossed finish first. pink_slip=true → bet was a pink slip.
    // Updates Reputation, RacerPool, and bankroll as appropriate.
    // Returns true if the King was beaten in a drag race (triggers end-game).
    bool OnRaceFinished(RaceType raceType, bool won, bool pink_slip = false);

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

    int  GetGameHours() const { return gameHours; }
    void AdvanceGameHours(int hours) { gameHours += hours; }
    std::string GetGameDateString() const;
    int  CalculateScore() const;

    Car* GetCurrentCar();
    void SetCurrentCar(Car *car);
    std::vector<Car *> GetOwnedCars();
    void AddOwnedCar(Car* car);

    const std::vector<CarWheelType>& GetSpareTires() const { return spareTires; }
    void AddSpareTire(CarWheelType type) { spareTires.push_back(type); }
    bool RemoveSpareTire(CarWheelType type) {
        auto it = std::find(spareTires.begin(), spareTires.end(), type);
        if (it == spareTires.end()) return false;
        spareTires.erase(it);
        return true;
    }

    Roster& GetRoster() { return roster; }
    std::vector<Car *>& GetClassifiedCars() { return classifiedCars; }
    CarData carData;

};

extern State state;
