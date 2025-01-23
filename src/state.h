#pragma once
#include "global.h"
#include "scene.h"
#include "car.h"
#include "cardata.h"
#include "diner.h"
#include "driving.h"
#include "garage.h"
#include "newspaper.h"
#include "gas_station.h"
#include "assets.h"
#include "roster.h"
#include "debug_overlay.h"
#include "racing.h"

#include <set>
#include <vector>
#include <string>
#include <SDL.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

class State {
private:
    int bankroll;
    // int dragRacesCompleted;
    // int roadRacesCompleted;
    bool debugMode;

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    std::vector<Car *> carRoster;
    std::vector<Car *> ownedCars;
    Car *currentCar;

    Roster roster;

    SrLibGameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;

    std::unique_ptr<Garage> garage;
    std::unique_ptr<Diner> diner;
    std::unique_ptr<GasStation> gasStation;
    std::unique_ptr<Driving> driving;
    std::unique_ptr<Racing> racing;
    std::unique_ptr<Newspaper> newspaper;

    // Remove this later
    const std::string GetTomlFile();

public:
    State();
    ~State();

    void Init(bool debugMode, const fs::path& gamePath);
    void InitWithCustomPath(bool debugMode, const std::string& customPath);

    int GetBankroll();

    SrLibGameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(SrLibGameScene scene);

    Opponent* GetNextOpponent();
    Opponent* GetCurrentOpponent();

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();

    Car* GetCurrentCar();
    void SetCurrentCar(Car *car);
    std::vector<Car *> GetOwnedCars();

    CarData carData;
    AssetManager assets;
    
};

extern State state;
