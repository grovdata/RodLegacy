#include "state.h"
#include "diner.h"
#include "garage.h"
#include "diner.h"
#include "gas_station.h"
#include "driving.h"

State::State() : bankroll(750) {
}

void State::Init(bool debugMode, const fs::path& gamePath) {
    this->debugMode = debugMode;
    assets.SetCustomPath(gamePath.string());
    try {
        assets.Init(gamePath);
        assets.ReadAssets();
        pair<int, vector<BitmapEntry_t>> entries = assets.GetImageEntries();   

        carData.ReadEntries(assets.GetSrData(), GetTomlFile(), entries);

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

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        SDL_Log("Current car tires: %d", this->currentCar->GetTires().assetId);
        
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
        //driving->Init();
    } catch (const std::exception& e) {
        SDL_Log("Failed to initialize game state: %s", e.what());
        // Clean up any resources that were allocated before the exception
        delete currentCar;
        throw; // Re-throw to main
    }
}

State::~State() = default;

const string State::GetTomlFile() {
    return "../data/cars.toml";
}

int State::GetBankroll() {
    return bankroll;
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

void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

Opponent* State::GetNextOpponent() {
    return roster.GetNextOpponent();
}

Opponent* State::GetCurrentOpponent() {
    return roster.GetCurrentOpponent();
}

// Global object
State state;
