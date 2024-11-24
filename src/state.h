#pragma once
#include "global.h"
#include "scene.h"
#include "car.h"
#include "cardata.h"
#include "diner.h"
#include "driving.h"
#include "garage.h"
#include "gas_station.h"
#include "assets.h"
#include "roster.h"
#include "debug_overlay.h"

#include <set>
#include <vector>
#include <string>
#include <SDL.h>
#include <memory>

class State {
private:
    int bankroll;
    int dragRacesCompleted;
    int roadRacesCompleted;
    bool debugMode;

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    std::vector<Car *> carRoster;
    Car *currentCar;

    Roster roster;

    GameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;

    std::unique_ptr<Garage> garage;
    std::unique_ptr<Diner> diner;
    std::unique_ptr<GasStation> gasStation;
    std::unique_ptr<Driving> driving;


public:
    State();
    ~State();

    void Init(bool enableDebug);

    int GetBankroll();

    GameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(GameScene scene);

    Opponent* GetNextOpponent();
    Opponent* GetCurrentOpponent();

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();

    Car* GetCurrentCar();

    CarData carData;
    AssetManager assets;
    
    const string GetTomlFile();
    const string GetDataFile();
    const string GetLibPFile();
    const string GetLibStartFile();
    
};

extern State state;
