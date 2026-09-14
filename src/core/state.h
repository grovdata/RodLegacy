#pragma once
#include "core/global.h"
#include "parsers/libp.h"  // For SrLibGameScene
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
class SaveLoadScene;
class GameOverMenu;
class CarDebug;

class State {
private:
    int bankroll;
    // int dragRacesCompleted;
    // int roadRacesCompleted;
    bool debugMode;
    std::string playerName = "Player";
    bool jailed = false;

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    Reputation reputation;
    RacerPool  racerPool;
    std::vector<SrDriverData>   srDriverData;
    std::vector<SrCarSpecEntry> srCarSpecData;

    // Every car in play, from every enabled set, in one table. The Amiga set
    // is always first so its entries stay at 0..25 - the indices original
    // .SAV files store - and optional sets append after it. This is what the
    // racer pool and roster draw from.
    std::vector<SrCarSpecEntry> mergedCarSpecs;
    // Pool key (CarModel::index) for each mergedCarSpecs entry, so a spec can
    // be turned back into a model. -1 for specs with no buyable car, i.e. The
    // King's slot in each set.
    std::vector<int>            mergedCarSpecModelIndex;
    int lastChallengerPoolIndex = -1;
    // Won the King's drag qualifier; his road race (for pink slips) is forced
    // next. Cleared by a loss to him in either race. See king.md.
    bool kingQualifierWon = false;
    // Set by RequestKingCameoRoll(), consumed by the next PickNextOpponent().
    bool pendingKingCameoRoll = false;

    RaceType pendingRaceType   = RaceType::Road;
    int      pendingBetAmount  = 50;
    bool     pendingIsPinkSlip = false;

    std::vector<CarPtr> carRoster;
    std::vector<CarPtr> ownedCars;
    std::vector<CarPtr> classifiedCars; // Cars for sale in the newspaper
    CarPtr currentCar;

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
    std::unique_ptr<SaveLoadScene> saveLoad;
    std::unique_ptr<GameOverMenu> gameOverMenu;
#ifdef RODLEGACY_DEBUG_UI
    std::unique_ptr<CarDebug> carDebug;
#endif

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
    Newspaper* GetNewspaper() const { return newspaper.get(); }
    SaveLoadScene* GetSaveLoadScene() const { return saveLoad.get(); }

    // Every enabled car set's specs in one table, Amiga set first. Parallel
    // to GetCarSpecModelIndex(), which maps each entry back to a pool key.
    const std::vector<SrCarSpecEntry>& GetMergedCarSpecs() const { return mergedCarSpecs; }
    const std::vector<int>& GetCarSpecModelIndex() const { return mergedCarSpecModelIndex; }

    void Init(bool debugMode, const fs::path& gamePath, bool muteAudio = false);
    void InitWithCustomPath(bool debugMode, const std::string& customPath);

    int GetBankroll();
    void SetBankroll(int amount);

    SrLibGameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(SrLibGameScene scene);

    // Clears the owned-car roster (frees every Car* State owns) and resyncs
    // Garage's Car*-keyed caches, leaving the player carless -- same as a
    // fresh Amiga boot, before the first classified-ad purchase. Called from
    // NameEntry::HandleOk, so it applies whether the flow started at launch
    // or via Garage's "New Game" button.
    void StartNewGame();

    // Nothing left to race and nothing left to sell: no car anywhere, no
    // spare parts, and less than the cheapest car in the paper. The original
    // checks exactly this on every return to the garage (FUN_0023fdb4, called
    // from the garage loop 0x23fe24) and ends the run - "You're outta dough."
    bool IsOutOfDough() const;

    // Fined more than you had: the run is over and the diner is seen from
    // inside a cell. See RodLegacyResearch/docs/diner.md ("Jail").
    bool IsJailed() const { return jailed; }
    void SetJailed(bool v) { jailed = v; }
    // For the debug UI checkbox (Diner::GetDebugPanels), so the jail view can
    // be previewed without losing a race to a cop first.
    bool* DebugJailedFlag() { return &jailed; }

    Opponent* GetNextOpponent();
    Opponent* GetCurrentOpponent();
    bool IsKingOpponent(const Opponent* opp) const { return roster.IsKing(opp); }

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

    // Builds the car an SR_data spec index describes, drivetrain and all.
    // Null when the index has no buyable car behind it (The King's slot).
    CarPtr BuildCarForSpec(int specIndex);

    // Ported from ShowOpponentRefusal (Amiga 0x0023e904): after an opponent
    // turns down a bet, the very next reroll has a flat 1/100 chance of
    // being The King, independent of reputation. Call once, right when the
    // rejected opponent's car starts driving off; the next PickNextOpponent()
    // consumes and clears it.
    void RequestKingCameoRoll() { pendingKingCameoRoll = true; }

    // One race against the current challenger is over. Call for every finished
    // race, a speeding ticket included - ChallengeRaceLoop bumps the pool
    // record's race counter outside its win/loss test. The King has no pool
    // record, so he is skipped. opponentWon is false for a win and for a
    // ticket, which voids the race.
    void RecordChallengerRace(bool opponentWon);

    // Call after a race finishes. won=true means the player crossed first.
    // Whether the stake was a pink slip is taken from the pending bet.
    // Updates Reputation, RacerPool, and bankroll as appropriate.
    // Returns true if the King was beaten in his road race (triggers end-game).
    bool OnRaceFinished(RaceType raceType, bool won);

    // Won the King's drag qualifier; his forced road race is next.
    bool IsKingQualifierWon() const { return kingQualifierWon; }

    Reputation& GetReputation() { return reputation; }
    // SR_data driver row (tier / reaction / aggression) for the challenger the
    // pool last selected, or nullptr when there is no pool-backed challenger.
    const SrDriverData* GetCurrentDriverData();
    int GetLastChallengerPoolIndex() const { return lastChallengerPoolIndex; }
    RacerPool&  GetRacerPool()  { return racerPool; }

    // Performance scores, for opponent selection and the debug overlay.
    int GetPlayerScore()     const;
    int GetChallengerScore() const;

    // SR_data car spec of the challenger the pool last selected (The King's
    // included), or nullptr when there is no pool-backed challenger.
    const SrCarSpecEntry* GetChallengerSpec() const;
    // Everything OpponentAcceptsBet needs about the current pairing.
    BetContext GetBetContext();

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
    // Takes fuel off the current car, floored at empty. The original burns it
    // in two places (see docs/driving/cop.md): 2 tenths per drive leg between
    // locations, in RunRoadRaceWithMode, and 5 (drag) / 15 (road) tenths per
    // race, in ProcessRaceOutcome.
    static constexpr int FUEL_PER_TRIP_TENTHS = 2;
    static constexpr int FUEL_PER_DRAG_RACE_TENTHS = 5;
    static constexpr int FUEL_PER_ROAD_RACE_TENTHS = 15;
    void BurnFuel(int tenths);
    const CarPtr& GetCurrentCarPtr() const { return currentCar; }
    void SetCurrentCar(CarPtr car);
    const std::vector<CarPtr>& GetOwnedCars() const { return ownedCars; }
    void AddOwnedCar(CarPtr car);
    // Drops one car from the roster. Does not touch the current car, so a
    // caller that sells the car it is driving clears that itself - the
    // original leaves the player carless in exactly that case.
    void RemoveOwnedCar(const CarPtr& car);
    // Drops the car being driven and falls back to whatever else is in the
    // garage - a forfeited pink slip, or a wreck the player junked. With
    // nothing left the player is carless and has to buy one from the paper.
    void ScrapCurrentCar();
    // What a car is worth, and what is left of that once the drivetrain has
    // been pulled off it (DismantleCar's return) - see Car::SetValue.
    int GetCarValue(const Car* car);
    int GetCarStrippedValue(const Car* car);

    // Loads a real Amiga .sav file, replacing bankroll, owned cars, known-
    // opponent car specs, race record, and elapsed time. Returns false
    // (state left unchanged) on a malformed file. See save_loader.h for
    // the parser and its documented gaps; RodLegacyResearch/docs/formats/
    // save_game.md is the authoritative format reference.
    bool LoadSaveGame(const fs::path& path);

    // RodLegacy's own save format (save_file.h): unlike the read-only .sav
    // path above, this round-trips everything State holds - player name,
    // spare parts, both reputation buckets and the full racer pool.
    // Both return false and leave State untouched on a bad file.
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

    // Drops one spare by category and position in that category's vector -
    // what the garage's spare-parts list needs, since Remove*(type) above
    // would take the first match rather than the row the player picked.
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
        }
        return false;
    }

    Roster& GetRoster() { return roster; }
    std::vector<CarPtr>& GetClassifiedCars() { return classifiedCars; }
    CarData carData;

};

extern State state;
