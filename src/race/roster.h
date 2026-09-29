#pragma once
#include <string>
#include <vector>
#include "race/driver.h"
#include "car/car.h"
#include "car/cardata.h"
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
        // Fallback only when SR_data is unavailable; it lacks Jim and Tom.
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
        // The opponent actually in play; currentOpponent has already advanced past them.
        Opponent* activeOpponent = nullptr;
        // Never part of opponents[]: SR_data driver 21 is excluded from the random roster.
        Opponent kingOpponent;
        bool kingOpponentBuilt = false;
        Driver GenerateRandomDriver(int preferredCarIndex);
    public:
        void SetDriverTable(const std::vector<SrDriverData>& drivers,
                            const std::vector<std::string>& names);
    private:
        void LoadCustomDrivers();
        void LoadCustomStickers();
    public:
        Roster();
        void Initialize(CarData& carData);
        CarPtr GenerateRandomCar(const CarModel& model);
        static void ApplyCarSpecFor(Car* car, const CarModel& model);
        Opponent* GetNextOpponent();
        Opponent* GetCurrentOpponent();
        bool HasMoreOpponents() const;
        void ShuffleOpponents();
        size_t GetOpponentCount() const { return opponents.size(); }

        void AddKnownOpponent(Opponent* opp);
        const std::vector<Opponent*>& GetKnownOpponents() const;
        Opponent* FindOpponentByName(const std::string& name);
        void SetNextOpponent(Opponent* opp);
        bool HasForcedOpponent() const { return forcedNextOpponent != nullptr; }
        Opponent* GetOpponentByIndex(int index);

        // Built lazily: a stock '63 Corvette (kingSpec) in his own paint.
        Opponent* GetKingOpponent(CarData& carData, const SrCarSpecEntry& kingSpec);

        // Compared by identity, since lastChallengerPoolIndex moves whenever a challenger is re-picked.
        bool IsKing(const Opponent* opp) const { return opp == &kingOpponent; }
};