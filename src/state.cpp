#include "state.h"
#include "diner.h"
#include "garage.h"
#include "gas_station.h"
#include "game.h"

State::State() : bankroll(750), dragRacesCompleted(0), roadRacesCompleted(0) {
    assets.ReadAssets(GetLibPFile());
    pair<int, vector<BitmapEntry_t>> entries = assets.GetImageEntries();
    carData.ReadEntries(this->GetDataFile(), GetTomlFile(), entries);

    // Initialize roster
    roster.Initialize(carData);
    
    CarModel randomModel = carData.GetRandomModel();
    Car *randomCar = new Car(randomModel);
    this->currentCar = randomCar;
    
    diner.Init();
    gasStation.Init();
    garage.Init();
    //driving.Init();
}

State::~State() {
    // delete garage;
    // delete diner;
    delete this->currentCar;
}

const string State::GetTomlFile() {
    return "../data/cars.toml";
}

const string State::GetDataFile() {
    return "../../streetrod-bins/amiga/SR_disk2/SR_data";
}

const string State::GetLibPFile() {
    return "../../streetrod-bins/amiga/SR_disk2/lib.p";
}

const string State::GetLibStartFile() {
    return "../../streetrod-bins/amiga/SR_disk1/libstart.p";
}

int State::GetBankroll() {
    return bankroll;
}

GameScene State::GetCurrentGameScene() {
    return currentGameScene;
}

Scene* State::GetCurrentScene() {
    return currentScene;
}

void State::SetCurrentScene(Scene *scene) {
    this->currentScene = scene;
}

void State::MoveToScene(GameScene scene) {
    currentGameScene = scene;
    switch (scene) {
        case SCENE_DINER:
            this->currentScene = &diner;
            break;
        case SCENE_GARAGE:
            this->currentScene = &garage;
            break;
        case SCENE_GAS_STATION:
            this->currentScene = &gasStation;
            break;
        case SCENE_CRUISING:
        case SCENE_DRIVING:
            this->currentScene = &driving;
            break;
    }

    this->currentScene->FadeIn();
}

Car* State::GetCurrentCar() {
    return this->currentCar;
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

// Global object
State state;
