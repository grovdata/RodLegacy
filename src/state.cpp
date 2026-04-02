#include "log.h"
#include "state.h"
#include "scene.h"
#include "car.h"
#include "diner.h"
#include "garage/garage.h"
#include "gas_station.h"
#include "driving.h"
#include "newspaper.h"
#include "racing.h"
#include "debug_overlay.h"
#include "parsers/libp.h"

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
        if (srDriverData.size() == SR_DATA_DRIVER_ENTRY_COUNT &&
            srCarSpecData.size() == SR_DATA_CARSPEC_ENTRY_COUNT) {
            racerPool.Init(srDriverData, srCarSpecData);
            LOG_INFO("RacerPool initialised: %d active opponents", racerPool.CountActive());
        } else {
            LOG_INFO("SR_data parse incomplete (drivers=%zu carspecs=%zu) — pool disabled",
                     srDriverData.size(), srCarSpecData.size());
        }

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

        // Generate classified cars for the newspaper
        for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
            int carAssetId = kCarIndex[i];
            CarMetadata_t metadata = carData.GetCarMetadata(carAssetId);
            if (metadata.classified.carName && strlen(metadata.classified.carName) > 0) {
                CarModel model = carData.GetModelFromIndex(carAssetId);
                Car* car = roster.GenerateRandomCar(model);
                // Stock appearance for classifieds
                car->SetRoofChopped(false);
                car->SetBumpersStripped(false);
                car->SetDecal(SrLibCarDecalNone);
                car->SetScoop(SCOOP_NONE);
                classifiedCars.push_back(car);
            }
        }
        LOG_INFO("Generated %zu classified cars", classifiedCars.size());

        // Seed spare tires for testing
        spareTires = { STOCK, BRAND, RACING };

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        LOG_INFO("Current car tires: %d", this->currentCar->GetTires().assetId);

        // Create scenes after fonts are initialized
        garage = std::make_unique<Garage>();
        diner = std::make_unique<Diner>();
        gasStation = std::make_unique<GasStation>();
        driving = std::make_unique<Driving>();
        newspaper = std::make_unique<Newspaper>();

        garage->Init(debugMode);
        diner->Init(debugMode);
        gasStation->Init(debugMode);
        newspaper->Init(debugMode);
        driving->Init(debugMode);
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        // Clean up any resources that were allocated before the exception
        delete currentCar;
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
            this->currentScene = newspaper.get();
            break;
    }

    this->currentScene->FadeIn();
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
}

void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

std::string State::GetGameDateString() const {
    // Game starts June 15, 1963 at midnight; gameHours=12 = noon that day.
    int dayOffset = gameHours / 24;

    static const struct { const char* name; int days; } months[] = {
        { "June",      30 },
        { "July",      31 },
        { "August",    31 },
        { "September", 30 },
    };
    // Day 0 = June 15
    int day = 15 + dayOffset;
    int m = 0;
    while (m < 3 && day > months[m].days) {
        day -= months[m].days;
        m++;
    }
    return std::string(months[m].name) + " " + std::to_string(day) + ", 1963";
}

int State::CalculateScore() const {
    // DOS formula: score = elapsed_ticks/546 + 12, where 546 ticks = 1 hour
    // and the game starts at noon June 15 (hour 12).
    // We track gameHours starting at 12, so score == gameHours directly.
    return gameHours;
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
    SrCarSpecEntry playerSpec{};
    if (currentCar) {
        playerSpec.engine       = static_cast<uint8_t>(currentCar->GetEngine().engineType);
        playerSpec.transmission = static_cast<uint8_t>(currentCar->GetGearbox().partType.transmissionType);
        playerSpec.carburetor   = static_cast<uint8_t>(currentCar->GetCarburator().partType.carburetorType);
        playerSpec.tires        = static_cast<uint8_t>(currentCar->GetWheelType());
        playerSpec.condition    = 0;

        // Reverse-lookup asset index -> carspec index to get street cred.
        int assetId = currentCar->GetModel().index;
        for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
            if (kCarIndex[i] == assetId && i < static_cast<int>(srCarSpecData.size())) {
                playerSpec.street_cred = srCarSpecData[i].street_cred;
                break;
            }
        }
    }
    return ComputeCarPerformanceScore(playerSpec);
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

bool State::OnRaceFinished(RaceType raceType, bool won, bool pink_slip) {
    if (won) {
        reputation.RecordWin(raceType);
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceWon(lastChallengerPoolIndex, pink_slip);
        }
        if (pendingIsPinkSlip) {
            // TODO: transfer opponent car to ownedCars
            LOG_INFO("State::OnRaceFinished - pink slip win, car transfer not yet implemented");
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
        if (pendingIsPinkSlip) {
            // TODO: remove current car from ownedCars and transfer to opponent
            LOG_INFO("State::OnRaceFinished - pink slip loss, car transfer not yet implemented");
        } else {
            bankroll = std::max(0, bankroll - pendingBetAmount);
        }
        LOG_INFO("State::OnRaceFinished - LOSS $%d bankroll=%d drag_rep=%d road_rep=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep);
    }
    return false;
}

// Global object
State state;
