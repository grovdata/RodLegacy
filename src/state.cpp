#include "state.h"
#include "diner.h"
#include "garage.h"
#include "gas_station.h"


State::State() : bankroll(750), dragRacesCompleted(0), roadRacesCompleted(0) {
    // this->garage = new Garage();
    // this->diner = new Diner();
    this->allDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};

    // TODO
    CarModel chevy = carData.GetModel(CAR_MODEL_GM_CHEVROLET_2DR);
    carData.PrintModel(chevy);
    Car *chevyCar = new Car(chevy);
    this->currentCar = chevyCar;
}

State::~State() {
    // delete garage;
    // delete diner;
    delete this->currentCar;
}

int State::GetBankroll() {
    return bankroll;
}

Scene* State::GetCurrentScene() {
    return currentScene;
}

void State::SetCurrentScene(Scene *scene) {
    this->currentScene = scene;
}

void State::MoveToScene(GameScene scene) {
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
    }
    // this->currentScene->FadeIn();
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

// Global object
State state;