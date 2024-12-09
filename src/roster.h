#pragma once
#include <string>
#include <vector>
#include "driver.h"
#include "car.h"
#include "cardata.h"

using namespace std;

class Opponent {
    public:
    Driver driver;
    Car* car;
    Opponent();
    ~Opponent() { }
    // ~Opponent() { delete car; }
    // Opponent(const Opponent&) = delete;
    // Opponent& operator=(const Opponent&) = delete;
    // Opponent(Opponent&&) = default;
    // Opponent& operator=(Opponent&&) = default;
};

class Roster {
    private:
        vector<string> stockDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
        vector<Opponent> opponents;
        vector<Opponent>::iterator currentOpponent;
        Driver GenerateRandomDriver(int preferredCarIndex);
    public:
        Roster();
        void Initialize(CarData& carData);
        Car* GenerateRandomCar(const CarModel& model);
        Opponent* GetNextOpponent();
        Opponent* GetCurrentOpponent();
        bool HasMoreOpponents() const;
        void ShuffleOpponents();
        size_t GetOpponentCount() const { return opponents.size(); }
};