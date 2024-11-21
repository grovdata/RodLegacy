#include "roster.h"
#include <random>
#include <algorithm>

Opponent::Opponent() : car(nullptr) {}

Roster::Roster() : currentOpponent(opponents.begin()) {}

void Roster::Initialize(CarData& carData) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrCar(0, CAR_INDEX_COUNT - 1);

    // Clear any existing opponents
    opponents.clear();

    // Create opponents from stock driver names
    while (!stockDriverNames.empty()) {
        int preferredCarIndex = CAR_INDEX[distrCar(gen)];
        std::cout << "Preferred car index: " << preferredCarIndex << std::endl;
        
        Driver driver = GenerateRandomDriver(preferredCarIndex);
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
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
    
    // Random color
    // PAL_CAR_COLOR_LIGHT_GREEN

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> wearDist(0, 30);
    std::uniform_int_distribution<> stickerDist(0, (int)CarDecal::CarDecalNone - 1);
    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);

    // Set random customization and wear
    car->SetDecal((CarDecal)stickerDist(gen));
    car->SetTires((CarWheelType)tireDist(gen));

    // TODO: Uncomment when part system is implemented
    // car->SetGearbox(new Part(RACING_HOOD, wearDist(gen)));
    // car->SetTires(new Part(RACING_HOOD, wearDist(gen)));
    // car->SetEngine(new Part(RACING_HOOD, wearDist(gen)));
    // car->SetCarburator(new Part(RACING_HOOD, wearDist(gen)));
    // car->SetManifold(new Part(RACING_HOOD, wearDist(gen)));

    return car;
}

Driver Roster::GenerateRandomDriver(int preferredCarIndex) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> spriteIndexDist(DRIVER_01, DRIVER_01 + DRIVER_COUNT - 1);
    Driver driver;
    driver.name = stockDriverNames.back();
    driver.spriteIndex = spriteIndexDist(gen);
    stockDriverNames.pop_back();
    driver.preferredCar = preferredCarIndex;
    return driver;
}

Opponent* Roster::GetNextOpponent() {
    if (currentOpponent == opponents.end()) {
        return nullptr;
    }
    return &*(currentOpponent++);
}

bool Roster::HasMoreOpponents() const {
    return currentOpponent != opponents.end();
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