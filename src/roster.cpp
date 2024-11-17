#include "roster.h"
#include <random>
#include <ctime>
#include "parsers/libp.h"

Opponent::Opponent() {

}

Roster::Roster() {

}

void Roster::Randomize(CarData *carData) {
    int stockCarCount = CAR_INDEX_COUNT;

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    std::uniform_int_distribution<> distrCar(0, stockCarCount);
    std::uniform_int_distribution<> distrName(0, stockDriverNames.size()-1);

    while (this->stockDriverNames.size() > 0) {
        string name = this->stockDriverNames.back();
        this->stockDriverNames.pop_back();
        int preferredCar = distrCar(gen);
        Driver driver;
        driver.preferredCar = CAR_INDEX[preferredCar];
        driver.name = name;
        std::cout << "Random name " << driver.name << " car " << driver.preferredCar << " (" << stockCarCount << ") " << this->stockDriverNames.size()  << std::endl;        
        Opponent oppo;
        oppo.driver = driver;
        opponents.push_back(oppo);
    }

    for (auto d: opponents) {        
        int carIndex = d.driver.preferredCar;
        CarModel model = carData->GetModelFromIndex(carIndex);
        Car* car = new Car(model);

        // Random number generation for parts wear and choices
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> wearDist(0, 30); // Parts start with some wear
        std::uniform_int_distribution<> stickerDist(0, (int)CarDecal::CarDecalNone - 1);
        std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);

        // Create parts with random wear levels
        // FIXME bogus asset
        // car->SetGearbox(new Part(RACING_HOOD, wearDist(gen)));
        // car->SetTires(new Part(RACING_HOOD, wearDist(gen)));
        // car->SetEngine(new Part(RACING_HOOD, wearDist(gen)));
        // car->SetCarburator(new Part(RACING_HOOD, wearDist(gen)));
        // car->SetManifold(new Part(RACING_HOOD, wearDist(gen)));

        // Set random decal and tire type
        // car->SetDecal((CarDecal)stickerDist(gen));
        // car->SetTireType((CarWheelType)tireDist(gen));

    }

}