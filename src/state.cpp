#include "state.h"
#include "diner.h"
#include "garage.h"


State::State() : bankroll(750), dragRacesCompleted(0), roadRacesCompleted(0) {
    // this->garage = new Garage();
    // this->diner = new Diner();
    this->allDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
}

State::~State() {
    // delete garage;
    // delete diner;
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

void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

// Global object
State state;
Garage garage;
Diner diner;