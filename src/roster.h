#pragma once
#include <string>
#include <vector>
#include "driver.h"
#include "car.h"
#include "cardata.h"
#include <filesystem>

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
        std::vector<std::string> stockDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
        std::vector<Opponent> opponents;
        std::vector<Opponent>::iterator currentOpponent;
        std::vector<std::string> customDriverPaths;
        std::vector<std::string> customStickerPaths;
        std::vector<Opponent*> knownOpponents;
        Opponent* forcedNextOpponent = nullptr;
        Driver GenerateRandomDriver(int preferredCarIndex);
        void LoadCustomDrivers();
        void LoadCustomStickers();
    public:
        Roster();
        void Initialize(CarData& carData);
        Car* GenerateRandomCar(const CarModel& model);
        Opponent* GetNextOpponent();
        Opponent* GetCurrentOpponent();
        bool HasMoreOpponents() const;
        void ShuffleOpponents();
        size_t GetOpponentCount() const { return opponents.size(); }

        void AddKnownOpponent(Opponent* opp);
        const std::vector<Opponent*>& GetKnownOpponents() const;
        Opponent* FindOpponentByName(const std::string& name);
        void SetNextOpponent(Opponent* opp);
        Opponent* GetOpponentByIndex(int index);
};