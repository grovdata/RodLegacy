#pragma once
#include <set>
#include <vector>
#include <string>
#include <SDL.h>

struct Driver;
class Scene;
class Car;

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

    int GetBankroll();

    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);

    void SetElapsedTime(Uint64 elapsedTime);
    Uint64 GetElapsedTime();


};
extern State state;