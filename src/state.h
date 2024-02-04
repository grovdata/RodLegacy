#pragma once
#include "global.h"
#include "scene.h"
#include "car.h"
#include "cardata.h"
#include "diner.h"
#include "garage.h"
#include "gas_station.h"

#include <set>
#include <vector>
#include <string>
#include <SDL.h>

class State {
private:
    int bankroll;
    [[maybe_unused]] int dragRacesCompleted;
    [[maybe_unused]] int roadRacesCompleted;

    std::set<std::string> allDriverNames;
    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    std::vector<Car *> carRoster;
    Car *currentCar;


    Scene *currentScene;
    Uint64 elapsedTime;

    CarData carData;
    Garage garage;
    Diner diner;
    GasStation gasStation;

public:
    State();
    ~State();

    int GetBankroll();

    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(GameScene scene);

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();

    Car* GetCurrentCar();

};

extern State state;