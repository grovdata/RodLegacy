#include "state.h"

State::State() : bankroll(750) {
    allDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
}

int State::GetBankroll() {
    return bankroll;
}

Scene* State::GetCurrentScene() {
    return currentScene;
}

void State::SetCurrentScene(Scene *scene) {
    currentScene = scene;
}

void State::SetElapsedTime(Uint64 elapsedTime) {
    elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return elapsedTime;
}

// Global object
State state;