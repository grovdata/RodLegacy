#pragma once

#include <ctime>
#include <vector>
#include <set>
#include <random>

#include "car.h"
#include "driver.h"
#include "global.h"
#include "utils.h"

class Game {
   private:
    int m_bankroll;



    set<std::string> m_allDriverNames;
    set<Driver *> m_allDrivers;
    set<Driver *> m_knownDrivers;

    void LoadDrivers() {
        m_allDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
    }

   public:
    Game() : m_bankroll(750) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }

    int GetBankroll() {
        return m_bankroll;
    }

    std::string GetRandomDriver() {
        return getRandomElement(m_allDriverNames);
    }

    void Update(Uint64 elapsedTime) {

    }
};