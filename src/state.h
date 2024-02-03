#pragma once
#include "global.h"
#include "scene.h"

#include <set>
#include <vector>
#include <string>
#include <SDL.h>

// struct Driver;
class Car;
class Diner;
class Garage;
class GasStation;

class State {
private:
    int bankroll;
    [[maybe_unused]] int dragRacesCompleted;
    [[maybe_unused]] int roadRacesCompleted;

    std::set<std::string> allDriverNames;
    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;

    std::vector<Car *> carRoster;
    [[maybe_unused]] Car *currentCar;


    Scene *currentScene;
    Uint64 elapsedTime;

public:
    State();
    ~State();

    int GetBankroll();

    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void MoveToScene(GameScene scene);

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();


};

extern State state;
extern Garage garage;
extern Diner diner;
extern GasStation gasStation;