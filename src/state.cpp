#include "log.h"
#include "state.h"
#include "scene.h"
#include "car.h"
#include "diner.h"
#include "garage/garage.h"
#include "gas_station.h"
#include "driving.h"
#include "newspaper.h"
#include "king_of_county.h"
#include "name_entry.h"
#ifdef RODLEGACY_DEBUG_UI
#include "car_debug.h"
#endif
#include "racing.h"
#include "debug_overlay.h"
#include "parsers/libp.h"
#include "parsers/adf.h"
#include "zip_utils.h"
#include "car_builder.h"
#include "save_loader.h"
#include <fstream>
#include <cstring>

State::State() : bankroll(5750) {
}

void State::Init(bool debugMode, const fs::path& gamePath, bool muteAudio) {
    this->debugMode = debugMode;
    try {
        assets.Init(gamePath, muteAudio);
        assets.ReadAssets();
        std::pair<int, std::vector<BitmapEntry_t>> entries = assets.GetImageEntries();   

        carData.ReadEntries(assets.GetSrData(), GetTomlFile(), entries);

        // Parse SR_data driver/car-spec tables and initialise pool.
        DataParser dataParser;

        srDriverData  = dataParser.ParseDriverData(assets.GetSrData());
        srCarSpecData = dataParser.ParseCarSpecTable(assets.GetSrData());

        // The Amiga set always goes in first, so its specs keep indices 0..25
        // - the range original .SAV files store - whatever else is switched on.
        mergedCarSpecs = srCarSpecData;
        mergedCarSpecModelIndex.assign(srCarSpecData.size(), -1);
        for (int i = 0; i < CAR_PLAYER_MODEL_COUNT && i < (int)srCarSpecData.size(); i++) {
            mergedCarSpecModelIndex[i] = kCarIndex[i];
        }

        // Then each optional set the player switched on. Everything a set
        // needs - stats, sprite numbers, pixel offsets - comes out of its own
        // data file; see RodLegacyResearch/docs/formats/hot_data_dos.md.
        const std::vector<CarSetDef>& carSetDefs = GetCarSetDefs();
        for (int set = 0; set < (int)carSetDefs.size(); set++) {
            if (carSetDefs[set].mandatory || !assets.IsCarSetEnabled(set)) continue;

            const byte_vector& setData = assets.GetCarSetData(set);
            const int added = carData.ReadCarSet(set, setData, assets.GetCarSetImageEntries(set));
            if (added <= 0) continue;

            const std::vector<SrCarSpecEntry> specs = dataParser.ParseCarSpecTableDOS(setData);
            const std::vector<DosCarSprites> sprites = dataParser.ParseCarSpritesDOS(setData);
            for (int i = 0; i < (int)specs.size(); i++) {
                mergedCarSpecs.push_back(specs[i]);
                // The King's slot in each set has no buyable car behind it.
                const bool buyable = i < CAR_PLAYER_MODEL_COUNT && i < (int)sprites.size();
                mergedCarSpecModelIndex.push_back(
                    buyable ? set * HOT_DATA_SPRITE_BASE + sprites[i].body : -1);
            }
            LOG_INFO("Car set '%s': %d cars, %zu specs", carSetDefs[set].id.c_str(), added, specs.size());
        }
        LOG_INFO("Merged car spec table: %zu entries across all enabled sets", mergedCarSpecs.size());

        if (srDriverData.size() == SR_DATA_DRIVER_ENTRY_COUNT &&
            srCarSpecData.size() == SR_DATA_CARSPEC_ENTRY_COUNT) {
            racerPool.Init(srDriverData, mergedCarSpecs);
            LOG_INFO("RacerPool initialised: %d active opponents", racerPool.CountActive());
        } else {
            LOG_INFO("SR_data parse incomplete (drivers=%zu carspecs=%zu) — pool disabled",
                     srDriverData.size(), srCarSpecData.size());
        }

        // Give the roster the real driver table before it builds opponents, so
        // names and portraits come from the data rather than a random sprite.
        roster.SetDriverTable(srDriverData, dataParser.ParseDriverNames(assets.GetSrData()));

        // Initialize roster
        roster.Initialize(carData);

        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_2DR_COUPE_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_ROADSTER_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_MASTER_DELUXE_2DR_1938)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1961)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1963)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(DODGE_CUSTOM_ROYAL_LANCER_1955)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_DEUCE_COUPE_1932)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(DODGE_POLARA_1962)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(PLYMOUTH_SAVOY_2DR_1962)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_SEDAN_1932)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_DELUXE_2DR_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_FAIRLANE_VICTORIA_1955)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_FAIRLANE_500_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(MERCURY_MONTEREY_1954)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(MERCURY_CUSTOM_2DR_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_STYLELINE_2DR_1949)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_IMPALA_1958)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(OLDSMOBILE_88_2DR_CUSTOM_1949)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(OLDSMOBILE_88_CUSTOM_1952)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(PONTIAC_SILVER_STREAK_1950)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_THUNDERBIRD_1957)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(VALIANT_V200_1961)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(VALIANT_V100_2DR_1963)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_BELAIR_SPORTS_COUPE)));

        // Generate classified cars for the newspaper: every buyable car from
        // every enabled set, in merged-table order. Newspaper::LoadClassifieds
        // walks the same table with the same skip condition, because the buy
        // handler indexes classifiedCars by the newspaper's list position.
        for (int spec = 0; spec < static_cast<int>(mergedCarSpecs.size()); spec++) {
            const int carAssetId = mergedCarSpecModelIndex[spec];
            if (carAssetId < 0) continue;  // The King's slot, not for sale
            CarMetadata_t metadata = carData.GetCarMetadata(carAssetId);
            if (!metadata.classified.carName || strlen(metadata.classified.carName) == 0) {
                continue;
            }
            CarModel model = carData.GetModelFromIndex(carAssetId);
            Car* car = roster.GenerateRandomCar(model);
            // Stock appearance for classifieds...
            car->SetRoofChopped(false);
            car->SetBumpersStripped(false);
            car->SetDecal(SrLibCarDecalNone);
            car->SetScoop(SCOOP_NONE);
            // ...and the drivetrain the car is actually advertised with.
            CarBuilder::ApplySpec(car, mergedCarSpecs[spec], model.vendor);
            classifiedCars.push_back(car);
        }
        LOG_INFO("Generated %zu classified cars", classifiedCars.size());

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        LOG_INFO("Current car tires: %d", this->currentCar->GetTires().assetId);

        // Create scenes after fonts are initialized
        garage = std::make_unique<Garage>();
        diner = std::make_unique<Diner>();
        gasStation = std::make_unique<GasStation>();
        driving = std::make_unique<Driving>();
        newspaper = std::make_unique<Newspaper>();
        nameEntry = std::make_unique<NameEntry>();
#ifdef RODLEGACY_DEBUG_UI
        carDebug = std::make_unique<CarDebug>();
#endif

        garage->Init(debugMode);
        diner->Init(debugMode);
        gasStation->Init(debugMode);
        newspaper->Init(debugMode);
        driving->Init(debugMode);
        nameEntry->Init(debugMode);
#ifdef RODLEGACY_DEBUG_UI
        carDebug->Init(debugMode);
#endif
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        throw; // Re-throw to main
    }
}

State::~State() = default;

const std::string State::GetTomlFile() {
    return "../data/cars.toml";
}

int State::GetBankroll() {
    return bankroll;
}

void State::SetBankroll(int amount) {
    bankroll = amount;
}

SrLibGameScene State::GetCurrentGameScene() {
    return currentGameScene;
}

Scene* State::GetCurrentScene() {
    return currentScene;
}

void State::SetCurrentScene(Scene *scene) {
    this->currentScene = scene;
}

void State::MoveToScene(SrLibGameScene scene) {
    const SrLibGameScene previousScene = currentGameScene;
    (void)previousScene;  // only read by the non-debug SCENE_CAR_DEBUG path
    if (currentScene) {
        currentScene->OnSceneExit();
    }
    currentGameScene = scene;
    switch (scene) {
        case SCENE_DINER:
            this->currentScene = diner.get();
            break;
        case SCENE_GARAGE:
            this->currentScene = garage.get();
            break;
        case SCENE_GAS_STATION:
            this->currentScene = gasStation.get();
            break;
        case SCENE_CRUISING:
        case SCENE_DRIVING:
            this->currentScene = driving.get();
            break;
        case SCENE_NEWSPAPER:
            if (newspaper) newspaper->ResetToFrontPage();
            this->currentScene = newspaper.get();
            break;
        case SCENE_KING_OF_COUNTY:
            kingOfCounty = std::make_unique<KingOfCounty>(true);
            kingOfCounty->Init(debugMode);
            this->currentScene = kingOfCounty.get();
            break;
        case SCENE_NAME_ENTRY:
            this->currentScene = nameEntry.get();
            break;
        case SCENE_CAR_DEBUG:
#ifdef RODLEGACY_DEBUG_UI
            this->currentScene = carDebug.get();
#else
            // Not built; stay where we are rather than null the scene.
            LOG_INFO("SCENE_CAR_DEBUG is only available in debug_ui builds");
            currentGameScene = previousScene;
            return;
#endif
            break;
    }

    this->currentScene->FadeIn();
}

void State::StartNewGame() {
    for (Car* c : ownedCars) delete c;
    ownedCars.clear();
    currentCar = nullptr;

    if (garage) garage->ResetForNewCarSet();
}

Car* State::GetCurrentCar() {
    return this->currentCar;
}

void State::SetCurrentCar(Car *car) {
    this->currentCar = car;
}

std::vector<Car *> State::GetOwnedCars() {
    return ownedCars;
}

void State::AddOwnedCar(Car* car) {
    ownedCars.push_back(car);

    // Garage is constructed once at startup and otherwise never refreshes
    // its owned-car bitmap cache (InitCarBitmaps only reruns via
    // ResetForNewCarSet, which nothing calls after a purchase) -- without
    // this, a car bought from the newspaper never appears in the garage.
    // Same pattern StartNewGame already uses to rebuild the cache.
    if (garage) garage->ResetForNewCarSet();
}

namespace {

// Builds one Car* from a parsed car record. Shared by the active car and
// every roster car in State::LoadSaveGame.
//
// rec.specIndex is the confirmed save-format index (0-25, sr_data.md's
// 26-entry car table). CarData::GetModelFromIndex does NOT take that index
// directly -- CarModel.index is set from kCarIndex[i] (libp.h), a separate
// lib.p bitmap-asset ID, not the plain spec position. kCarIndex[] is
// positionally ordered to match sr_data.md exactly (confirmed: index 0 =
// CHEVROLET_2DR_COUPE_1940 ... 24 = CHEVROLET_BELAIR_SPORTS_COUPE), but
// only holds the 25 player-ownable cars -- The King (spec index 25) isn't
// in it, since his car is never actually owned by the player in practice.
Car* BuildCarFromRecord(const LoadedCarRecord& rec, CarData& carData,
                        const std::vector<SrCarSpecEntry>& specs) {
    if (rec.specIndex < 0 || rec.specIndex >= CAR_INDEX_COUNT) {
        LOG_INFO("LoadSaveGame: car spec index %d has no known lib.p asset (King's car or out of range), using a placeholder model", rec.specIndex);
    }
    int assetId = (rec.specIndex >= 0 && rec.specIndex < CAR_INDEX_COUNT) ? kCarIndex[rec.specIndex] : kCarIndex[0];
    CarModel model = carData.GetModelFromIndex(assetId);
    Car* car = CarBuilder::BuildCar(model);

    // The saved car keeps its own parts (loaded below), but the class byte is
    // a property of the model, not of the save, so it comes from the spec.
    if (rec.specIndex >= 0 && rec.specIndex < static_cast<int>(specs.size())) {
        car->SetWeightClass(specs[rec.specIndex].car_class);
    }

    car->SetColor(static_cast<StandardCarColor>(rec.colorIndex));
    car->SetRoofChopped((rec.bodyByte & 0x04) != 0);
    car->SetBumpersStripped((rec.bodyByte & 0x01) != 0 && (rec.bodyByte & 0x02) != 0);
    car->SetDecal(static_cast<SrLibCarDecal>(rec.bodyByte >> 4));
    // Confirmed save byte is signed -8..+4, 0=perfectly tuned. RodLegacy's
    // own tuneLevel is used elsewhere as abs(tuneLevel-9) clamped 0..8
    // (racing.cpp), i.e. 9 is its own "perfectly tuned" center -- this
    // mapping stays compatible with that formula on both sides of the range.
    car->SetTuneLevel(9 + rec.tuningKnob);

    // Sub-list slot -> category, confirmed in save_game.md. Only a handful
    // of variant indices are confirmed to a specific named part (see that
    // doc); everything else keeps a vendor-appropriate default type but
    // still carries the real wear/cost across, per this session's plan.
    for (int slot = 0; slot < 7; ++slot) {
        if (rec.sublists[slot].empty()) continue;
        const LoadedPartRecord& p = rec.sublists[slot][0];
        switch (slot) {
            case 0: { // Transmission
                Part part = CarBuilder::CreateTransmissionPart(TRANS_4SPD_UNIVERSAL, model.vendor);
                part.wear = p.wearRaw / 100;
                part.cost = p.value;
                car->SetGearbox(part);
                break;
            }
            case 1: { // Engine
                Part enginePart(p.wearRaw / 100, p.value, model.vendor, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
                car->SetEngine(enginePart); // EnginePart::FromPart picks a vendor-appropriate type
                break;
            }
            case 2: { // Manifold
                Part part = CarBuilder::CreateManifoldPart(MANIFOLD_2BARREL, model.vendor);
                part.wear = p.wearRaw / 100;
                part.cost = p.value;
                car->SetManifold(part);
                break;
            }
            case 3: { // Carb
                Part part = CarBuilder::CreateCarburetorPart(CARB_2BARREL, model.vendor);
                part.wear = p.wearRaw / 100;
                part.cost = p.value;
                car->SetCarburator(part);
                break;
            }
            case 4:
            case 5:
                LOG_INFO("LoadSaveGame: car sub-list slot %d has data but no known category, skipping", slot);
                break;
            case 6: // Tires
                // Car::SetTires only takes a bare CarWheelType (no wear/cost
                // input), unlike gearbox/manifold/carburetor -- and the raw
                // variant byte has no confirmed CarWheelType mapping either,
                // so this loads a fixed default rather than guessing.
                car->SetTires(STOCK);
                break;
            default:
                break;
        }
    }
    return car;
}

} // namespace

bool State::LoadSaveGame(const fs::path& path) {
    LoadedSave loaded;
    try {
        loaded = SaveGameLoader::Load(path);
    } catch (const std::exception& e) {
        LOG_INFO("LoadSaveGame failed: %s", e.what());
        return false;
    }

    SetBankroll(loaded.bankroll);
    SetElapsedTicks(loaded.elapsedTicks);

    // Replace the owned-car roster. These are always freshly built here (no
    // pre-existing load path reuses them), so it's safe to free the old set.
    for (Car* c : ownedCars) delete c;
    ownedCars.clear();
    currentCar = nullptr;

    if (loaded.hasActiveCar) {
        Car* active = BuildCarFromRecord(loaded.activeCar, carData, srCarSpecData);
        ownedCars.push_back(active);
        currentCar = active;
    }
    for (const auto& rec : loaded.roster) {
        ownedCars.push_back(BuildCarFromRecord(rec, carData, srCarSpecData));
    }
    if (currentCar == nullptr && !ownedCars.empty()) currentCar = ownedCars.at(0);

    // Known opponents (trailer chunk 6): slot 21 is The King, who has no
    // mutable storage in RacerPool (RACER_KING_SLOT is a sentinel, not a
    // pool[] entry) -- only slots 0-20 are loaded.
    for (int i = 0; i < RACER_POOL_SIZE; ++i) {
        racerPool.SetCarSpecForSlot(i, loaded.opponentCarSpec[i]);
        if (i < static_cast<int>(racerPool.pool.size())) racerPool.pool[i].status = 0;
    }

    // Race record. The save doesn't distinguish drag vs. road wins, so the
    // full win count is attributed to drag_wins; races_won() (the only
    // thing read elsewhere) is unaffected by which bucket it's in.
    reputation = Reputation();
    reputation.races_completed = loaded.racesEntered;
    reputation.drag_wins = loaded.racesWon;

    LOG_INFO("LoadSaveGame: loaded '%s', bankroll=%d, %d owned car(s), %zu spare part(s) not sorted by category (see save_loader.h)",
              loaded.name.c_str(), loaded.bankroll, (int)ownedCars.size(), loaded.spareParts.size());
    return true;
}

void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

std::string State::GetGameClockString() const {
    return GameClock::FormatClock(elapsedTicks);
}

int State::CalculateScore() const {
    // Amiga: elapsedTicks advances 1 tick every 6 VBlanks (150 ticks/hour,
    // confirmed this session; DOS's own tick rate is different, 546/hour,
    // and not assumed to carry over). Epoch is midnight June 16 (hour 0),
    // so hours elapsed is a direct, un-offset proxy for time survived.
    return GameClock::TicksToHours(elapsedTicks);
}

Opponent* State::GetNextOpponent() {
    return roster.GetNextOpponent();
}

Opponent* State::GetCurrentOpponent() {
    return roster.GetCurrentOpponent();
}

void State::AddKnownOpponent(Opponent* opp) {
    roster.AddKnownOpponent(opp);
}

const std::vector<Opponent*>& State::GetKnownOpponents() const {
    return roster.GetKnownOpponents();
}

void State::SetNextOpponent(Opponent* opp) {
    roster.SetNextOpponent(opp);
}

int State::GetPlayerScore() const {
    // Scored the same way an opponent's car spec is (see racer_pool.cpp): the
    // car's top speed in mph. SrCarSpecEntry is not involved - it is a raw
    // 14-byte SR_data record and has no room for player state - so this goes
    // through the Car's own parts instead.
    if (!currentCar) {
        return srrace::ComputeTopSpeed(srrace::CarSetup{});
    }
    return currentCar->GetTopSpeed();
}

const SrDriverData* State::GetCurrentDriverData() {
    // The opponent carries the SR_data row it was built from, which is valid
    // whoever selected them. The pool index only exists when the pool picked
    // this challenger, so it is the fallback rather than the primary lookup.
    const Opponent* opp = GetCurrentOpponent();
    if (opp != nullptr && opp->driver.srDriverIndex >= 0 &&
        opp->driver.srDriverIndex < static_cast<int>(srDriverData.size())) {
        return &srDriverData[opp->driver.srDriverIndex];
    }

    if (lastChallengerPoolIndex < 0 || lastChallengerPoolIndex >= RACER_POOL_SIZE ||
        lastChallengerPoolIndex >= static_cast<int>(racerPool.pool.size())) {
        return nullptr;
    }
    const int driverIndex = racerPool.pool[lastChallengerPoolIndex].driver_index;
    if (driverIndex < 0 || driverIndex >= static_cast<int>(srDriverData.size())) {
        return nullptr;
    }
    return &srDriverData[driverIndex];
}

int State::GetChallengerScore() const {
    if (lastChallengerPoolIndex < 0 ||
        lastChallengerPoolIndex >= RACER_POOL_SIZE) return 70;
    return racerPool.GetEntryScore(lastChallengerPoolIndex);
}

void State::PickNextOpponent() {
    // Fall back to Roster shuffle if pool is not initialised.
    if (racerPool.pool.empty()) {
        return;
    }

    int playerScore = GetPlayerScore();

    // Check for King unlock.
    bool kingUnlocked = reputation.IsKingUnlocked(racerPool.CountActive());
    if (kingUnlocked) {
        LOG_INFO("State::PickNextOpponent - The King is unlocked");
        // King challenge uses the existing roster's next opponent slot for now.
        // Full King flow wired in OnRaceFinished.
        lastChallengerPoolIndex = RACER_KING_SLOT;
        return;
    }

    int poolIdx = racerPool.SelectNextChallenger(playerScore, lastChallengerPoolIndex);
    if (poolIdx < 0) {
        LOG_INFO("State::PickNextOpponent - pool exhausted, falling back to roster");
        return;
    }
    lastChallengerPoolIndex = poolIdx;

    // Map pool entry → Roster Opponent so existing Diner code works unchanged.
    RacerEntry& entry = racerPool.pool[poolIdx];
    int driverIdx = std::min(entry.driver_index, (int)roster.GetOpponentCount() - 1);
    Opponent* opp = roster.GetOpponentByIndex(driverIdx);
    if (opp) {
        roster.SetNextOpponent(opp);
        LOG_INFO("State::PickNextOpponent - pool[%d] driver=%d car_spec=%d score=%d",
                 poolIdx, entry.driver_index, entry.car_spec_index, playerScore);
    }
}

bool State::OnRaceFinished(RaceType raceType, bool won) {
    const bool pinkSlip = pendingIsPinkSlip;
    if (won) {
        reputation.RecordWin(raceType);
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceWon(lastChallengerPoolIndex, pinkSlip);
        }
        if (pinkSlip) {
            // The stake was the cars: take theirs. The pointer moves rather than
            // being copied, and the opponent is left without one so the same Car
            // is never owned twice - they have just lost it, after all.
            Opponent* opponent = GetCurrentOpponent();
            if (opponent != nullptr && opponent->car != nullptr) {
                Car* wonCar = opponent->car;
                // The pointer moves rather than being copied: Opponent does not
                // own its car, so leaving it in place would give one Car two
                // owners. They have just lost it in any case.
                opponent->car = nullptr;
                ownedCars.push_back(wonCar);
                // Drive it away: winning the pink slip means it is yours now,
                // and the old car stays in the garage alongside it.
                currentCar = wonCar;
                LOG_INFO("State::OnRaceFinished - pink slip win: now driving %s",
                         wonCar->GetModel().name.c_str());
            } else {
                LOG_INFO("State::OnRaceFinished - pink slip win but no opponent car to take");
            }
        } else {
            bankroll += pendingBetAmount;
        }
        LOG_INFO("State::OnRaceFinished - WIN $%d bankroll=%d drag_rep=%d road_rep=%d pool_active=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep,
                 racerPool.CountActive());

        // King beaten in drag race → end-game.
        if (lastChallengerPoolIndex == RACER_KING_SLOT && raceType == RaceType::Drag) {
            LOG_INFO("State::OnRaceFinished - King beaten in drag! Triggering end-game.");
            return true;
        }
    } else {
        reputation.RecordLoss();
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceLost(lastChallengerPoolIndex);
        }
        if (pinkSlip) {
            // The stake was the cars, so the one that was raced is gone. It is
            // dropped rather than deleted: nothing else in the game deletes a
            // Car, and the roster may still be holding this pointer.
            Car* lostCar = currentCar;
            if (lostCar != nullptr) {
                ownedCars.erase(std::remove(ownedCars.begin(), ownedCars.end(), lostCar),
                                ownedCars.end());
                // Fall back to whatever else is in the garage. With nothing
                // left the player is carless and has to buy one from the
                // paper, which is what the original does with its single car.
                currentCar = ownedCars.empty() ? nullptr : ownedCars.front();
                LOG_INFO("State::OnRaceFinished - pink slip loss: forfeited %s, %zu car(s) left",
                         lostCar->GetModel().name.c_str(), ownedCars.size());
            }
        } else {
            bankroll = std::max(0, bankroll - pendingBetAmount);
        }
        LOG_INFO("State::OnRaceFinished - LOSS $%d bankroll=%d drag_rep=%d road_rep=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep);
    }

    return false;
}

static constexpr int HALL_DAT_ENTRY_COUNT  = 10;
static constexpr int HALL_DAT_NAME_LEN     = 16;
static constexpr int HALL_DAT_SCORE_LEN    = 7;
static constexpr int HALL_DAT_ENTRY_SIZE   = HALL_DAT_NAME_LEN + HALL_DAT_SCORE_LEN; // 23
static constexpr int HALL_DAT_HEADER_SIZE  = 2;
static constexpr int HALL_DAT_FILE_SIZE    = HALL_DAT_HEADER_SIZE + HALL_DAT_ENTRY_COUNT * HALL_DAT_ENTRY_SIZE; // 232
static constexpr uint8_t HALL_DAT_XOR_KEY  = 0x80;

static std::vector<State::HallEntry> ParseHallDat(const uint8_t* data, size_t size) {
    std::vector<State::HallEntry> entries;
    if (size < (size_t)HALL_DAT_HEADER_SIZE) return entries;

    // First 2 bytes: big-endian int16 entry count (Amiga native, unencoded)
    int count = (int)((data[0] << 8) | data[1]);
    if (count <= 0 || count > HALL_DAT_ENTRY_COUNT) count = HALL_DAT_ENTRY_COUNT;

    // Only iterate as many complete entries as fit in the file
    int entries_that_fit = ((int)size - HALL_DAT_HEADER_SIZE) / HALL_DAT_ENTRY_SIZE;
    if (count > entries_that_fit) count = entries_that_fit;

    // Remaining bytes are XOR'd with 0x80
    for (int i = 0; i < count; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        char name[HALL_DAT_NAME_LEN + 1] = {};
        char score[HALL_DAT_SCORE_LEN + 1] = {};
        for (int b = 0; b < HALL_DAT_NAME_LEN; b++)
            name[b] = (char)(data[offset + b] ^ HALL_DAT_XOR_KEY);
        for (int b = 0; b < HALL_DAT_SCORE_LEN; b++)
            score[b] = (char)(data[offset + HALL_DAT_NAME_LEN + b] ^ HALL_DAT_XOR_KEY);
        if (name[0] == '\0') continue;
        State::HallEntry entry;
        entry.name  = name;
        entry.score = std::atoi(score);
        entries.push_back(entry);
    }
    return entries;
}

std::vector<State::HallEntry> State::LoadHallDat() const {
    // 1. Try local hall_dat file
    fs::path localPath = assets.GetBasePath() / "hall_dat";
    if (fs::exists(localPath)) {
        std::ifstream f(localPath, std::ios::binary);
        if (f) {
            std::vector<uint8_t> buf(HALL_DAT_FILE_SIZE, 0);
            f.read(reinterpret_cast<char*>(buf.data()), HALL_DAT_FILE_SIZE);
            auto entries = ParseHallDat(buf.data(), (size_t)f.gcount());
            LOG_INFO("LoadHallDat: loaded %zu entries from local hall_dat", entries.size());
            return entries;
        }
    }

    // 2. Fall back to ADF disk2 (direct file or inside sr1amiga.zip)
    const char* disk2Name = SrLibArchiveNames[SrLibArchiveSR1AmigaDisk2];
    fs::path disk2Path = assets.GetBasePath() / disk2Name;
    AdfFileData* adfData = nullptr;

    if (fs::exists(disk2Path)) {
        adfData = extract_adf_file_to_memory(disk2Path.string().c_str(), "hall_dat");
    } else {
        fs::path zipPath = assets.GetBasePath() / "sr1amiga.zip";
        if (fs::exists(zipPath)) {
            std::vector<uint8_t> disk2Buf = ZipExtractToMemory(zipPath, disk2Name);
            if (!disk2Buf.empty())
                adfData = extract_adf_file_from_buffer(disk2Buf.data(), disk2Buf.size(), "hall_dat");
        }
    }

    if (adfData) {
        auto entries = ParseHallDat(adfData->data, adfData->size);
        free_adf_file_data(adfData);
        LOG_INFO("LoadHallDat: loaded %zu entries from ADF disk2", entries.size());
        return entries;
    }

    LOG_INFO("LoadHallDat: hall_dat not found, starting with empty table");
    return {};
}

void State::SaveHallDat(const std::vector<HallEntry>& entries) const {
    uint8_t buf[HALL_DAT_FILE_SIZE] = {};

    // Header: int16 entry count (big-endian, Amiga native, unencoded)
    buf[0] = (HALL_DAT_ENTRY_COUNT >> 8) & 0xFF;
    buf[1] = HALL_DAT_ENTRY_COUNT & 0xFF;

    // Write up to 10 entries, XOR'd with 0x80
    int count = std::min((int)entries.size(), HALL_DAT_ENTRY_COUNT);
    for (int i = 0; i < count; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        // name[16]
        for (int b = 0; b < HALL_DAT_NAME_LEN; b++) {
            char c = (b < (int)entries[i].name.size()) ? entries[i].name[b] : '\0';
            buf[offset + b] = (uint8_t)(c ^ HALL_DAT_XOR_KEY);
        }
        // score[7] as decimal string
        char scoreBuf[HALL_DAT_SCORE_LEN + 1] = {};
        snprintf(scoreBuf, sizeof(scoreBuf), "%d", entries[i].score);
        for (int b = 0; b < HALL_DAT_SCORE_LEN; b++) {
            buf[offset + HALL_DAT_NAME_LEN + b] = (uint8_t)(scoreBuf[b] ^ HALL_DAT_XOR_KEY);
        }
    }
    // Remaining slots: XOR'd zero bytes = 0x80 for each byte
    for (int i = count; i < HALL_DAT_ENTRY_COUNT; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        for (int b = 0; b < HALL_DAT_ENTRY_SIZE; b++)
            buf[offset + b] = HALL_DAT_XOR_KEY;
    }

    fs::path localPath = assets.GetBasePath() / "hall_dat";
    std::ofstream f(localPath, std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(buf), HALL_DAT_FILE_SIZE);
        LOG_INFO("SaveHallDat: saved %d entries to %s", count, localPath.string().c_str());
    } else {
        LOG_INFO("SaveHallDat: failed to write %s", localPath.string().c_str());
    }
}

// Global object
State state;
