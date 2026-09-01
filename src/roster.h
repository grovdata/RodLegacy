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
    CarPtr car;
    Opponent();
    ~Opponent() { }
};

class Roster {
    private:
        // Filled from SR_data by SetDriverTable(); each entry carries the
        // driver's name and the portrait the data assigns them. Falls back to
        // this hardcoded list only when SR_data is unavailable - note it is
        // missing Jim and Tom, so it is not a faithful roster.
        std::vector<std::string> stockDriverNames = {"Mike", "Ralph", "Kirk", "Biff", "Butch", "Wally", "Chip", "Eddie", "Ernie", "Dick", "Steve", "John", "Charlie", "George", "Clyde", "Erica", "Spike", "Bill", "Vinnie"};
        std::vector<SrDriverData> srDrivers;
        std::vector<std::string> srDriverNames;
        int nextSrDriver = 0;
        std::vector<Opponent> opponents;
        std::vector<Opponent>::iterator currentOpponent;
        std::vector<std::string> customDriverPaths;
        std::vector<std::string> customStickerPaths;
        std::vector<Opponent*> knownOpponents;
        Opponent* forcedNextOpponent = nullptr;
        // Whoever GetNextOpponent() last handed out - the opponent actually in
        // play. Not derivable from currentOpponent, which has already advanced
        // past them, and which a forced pick bypasses entirely.
        Opponent* activeOpponent = nullptr;
        Driver GenerateRandomDriver(int preferredCarIndex);
    public:
        // Supply the parsed SR_data driver table before Initialize() so drivers
        // get their real names and portraits instead of random sprites.
        void SetDriverTable(const std::vector<SrDriverData>& drivers,
                            const std::vector<std::string>& names);
    private:
        void LoadCustomDrivers();
        void LoadCustomStickers();
    public:
        Roster();
        void Initialize(CarData& carData);
        CarPtr GenerateRandomCar(const CarModel& model);
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