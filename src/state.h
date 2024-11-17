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

#include <set>
#include <vector>
#include <string>
#include <SDL.h>

class State {
private:
    int bankroll;
    int dragRacesCompleted;
    int roadRacesCompleted;

    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    std::vector<Car *> carRoster;
    Car *currentCar;

    Roster roster;

    GameScene currentGameScene;
    Scene *currentScene;

    Uint64 elapsedTime;

    Garage garage;
    Diner diner;
    GasStation gasStation;
    Driving driving;

public:
    State();
    ~State();

    int GetBankroll();

    GameScene GetCurrentGameScene();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(GameScene scene);

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
