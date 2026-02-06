#pragma once
#include "global.h"
#include "parsers/libp.h"  // For SrLibGameScene
#include "assets.h"
#include "roster.h"
#include "cardata.h"

#include <set>
#include <vector>
#include <string>
#include <SDL.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Forward declarations
class Scene;
class Car;
class Diner;
class Driving;
class Garage;
class Newspaper;
class GasStation;
class Racing;

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

    // AssetManager first so it is destroyed last; scenes hold non-owning refs to its assets
    AssetManager assets;

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

    AssetManager& GetAssets() { return assets; }

    void Init(bool debugMode, const fs::path& gamePath, bool muteAudio = false);
    void InitWithCustomPath(bool debugMode, const std::string& customPath);

    int GetBankroll();
    void SetBankroll(int amount);

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

};

extern State state;
