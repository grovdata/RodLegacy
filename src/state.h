#pragma once
#include <set>
#include <string>

class Driver;
class Scene;

class State {
private:
    int bankroll;
    std::set<std::string> allDriverNames;
    std::set<Driver *> allDrivers;
    std::set<Driver *> knownDrivers;
    Scene *currentScene;

public:
    State();
    Scene* GetCurrentScene();
    void SetCurrentScene(Scene *scene);
};

extern State state;