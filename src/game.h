#pragma once

#include <ctime>
#include <random>
#include <set>
#include <vector>

#include "global.h"
#include "utils.h"

class Game {
   private:
    std::set<std::string> m_allDriverNames;
    std::set<Driver *> m_allDrivers;
    std::set<Driver *> m_knownDrivers;

    void LoadDrivers() {
        m_allDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
    }

   public:
    Game() {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
    }

    std::string GetRandomDriver() {
        return getRandomElement(m_allDriverNames);
    }

    void Update(Uint64 elapsedTime) {
    }
};