#pragma once
#include <string>
#include <vector>
#include "driver.h"
#include "car.h"

using namespace std;

class Opponent {
    public:
    Driver driver;
    Car car;
    Opponent();
};

class Roster {
    private:
        vector<string> stockDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
        vector<Opponent> opponents;
    public:
        Roster();
        void Randomize();

};