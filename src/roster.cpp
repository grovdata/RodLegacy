#include "roster.h"
#include "parsers/carcolor.h"
#include "state.h"
#include <random>
#include <algorithm>

Opponent::Opponent() : car(nullptr) {}

Roster::Roster() : currentOpponent(opponents.begin()) {}

void Roster::Initialize(CarData& carData) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrCar(0, CAR_INDEX_COUNT - 1);

    opponents.clear();
    
    while (!stockDriverNames.empty()) {
        int preferredCarIndex = CAR_INDEX[distrCar(gen)];        
        
        Driver driver = GenerateRandomDriver(preferredCarIndex);
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
        // model.Log();
        Car* car = GenerateRandomCar(model);

        // Create and add opponent directly to avoid move issues
        opponents.emplace_back();
        opponents.back().driver = std::move(driver);
        opponents.back().car = car;
    }

    ShuffleOpponents();
    currentOpponent = opponents.begin();
}

Car* Roster::GenerateRandomCar(const CarModel& model) {
    Car* car = new Car(model);
    
    // Random color selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> colorDist(0, StandardCarColor::CarColorCount - 1);
    StandardCarColor randomColor = static_cast<StandardCarColor>(colorDist(gen));
    
    // Set the car color
    car->SetColor(randomColor);

    // Update the asset with the selected color
    state.assets.UpdateAssetWithCarPalette(state.GetLibPFile(), model.index, randomColor); 

    std::uniform_int_distribution<> wearDist(0, 30);
    std::uniform_int_distribution<> stickerDist(0, (int)CarDecal::CarDecalNone - 1);
    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);
    std::uniform_int_distribution<> scoopDist(0, (int)CarScoopType::SCOOP_NONE - 1);

    // Set random customization and wear
    // Introduce a 25% chance to set CarDecal to CarDecalNone
    std::uniform_int_distribution<> chanceDist(0, 3); // 0 to 3 for 25% chance
    if (chanceDist(gen) == 0) {
        car->SetDecal(CarDecal::CarDecalNone);
    } else {
        car->SetDecal((CarDecal)stickerDist(gen));
    }
    
    car->SetTires((CarWheelType)tireDist(gen));
    car->SetScoop((CarScoopType)scoopDist(gen));
    // Introduce a 50% chance to not have a scoop at all
    if (chanceDist(gen) == 0) {
        car->SetScoop(CarScoopType::SCOOP_NONE);
    } else {
        car->SetScoop((CarScoopType)scoopDist(gen));
    }
    return car;
}

Driver Roster::GenerateRandomDriver(int preferredCarIndex) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> spriteIndexDist(DRIVER_01, DRIVER_01 + DRIVER_COUNT - 1);
    Driver driver;
    driver.name = stockDriverNames.back();
    if (driver.name == "Erica") {
        driver.spriteIndex = DRIVER_22;
    } else {
        do {
            driver.spriteIndex = spriteIndexDist(gen);
        } while (driver.spriteIndex == DRIVER_22);
    }
    stockDriverNames.pop_back();
    driver.preferredCar = preferredCarIndex;
    return driver;
}

Opponent* Roster::GetNextOpponent() {
    if (currentOpponent == opponents.end()) {
        currentOpponent = opponents.begin();  // Wrap around to start
    }
    return &*(currentOpponent++);
}

bool Roster::HasMoreOpponents() const {
    return !opponents.empty();  // Only return false if there are no opponents at all
}

void Roster::ShuffleOpponents() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(opponents.begin(), opponents.end(), gen);
    currentOpponent = opponents.begin();
}

Opponent* Roster::GetCurrentOpponent() {
    if (currentOpponent != opponents.end()) {
        return &(*currentOpponent);
    }
    return nullptr;
}