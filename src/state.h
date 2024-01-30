#pragma once
#include <set>
#include <string>
#include <SDL.h>

class Driver;
class Scene;

class State {
private:
    int bankroll;
    std::set<std::string> allDriverNames;
    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;
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