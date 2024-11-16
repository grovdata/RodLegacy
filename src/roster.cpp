#include "roster.h"
#include <random>
#include <ctime>
#include "parsers/libp.h"

Opponent::Opponent() {

}

Roster::Roster() {

}

void Roster::Randomize() {
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
        driver.name = stockDriverNames[distrName(gen)];
        std::cout << "Random name " << driver.name << " car " << driver.preferredCar << " (" << stockCarCount << ") " << this->stockDriverNames.size()  << std::endl;

        Opponent oppo;
        oppo.driver = driver;
        opponents.push_back(oppo);
    }

    #if 0

    for (auto d: opponents) {
        // FIXME
        Car* car = new Car((CarModel)d.driver.preferredCar);

        // Random number generation for parts wear and choices
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> wearDist(0, 30); // Parts start with some wear
        std::uniform_int_distribution<> stickerDist(0, (int)CarDecal::LAST - 1);
        std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);

        // Create parts with random wear levels
        car->gearbox = new Part(PartType::GEARBOX, wearDist(gen));
        car->tires = new Part(PartType::TIRES, wearDist(gen));
        car->engine = new Part(PartType::ENGINE, wearDist(gen));
        car->carburator = new Part(PartType::CARBURATOR, wearDist(gen));
        car->manifold = new Part(PartType::MANIFOLD, wearDist(gen));

        // Set random decal and tire type
        car->SetDecal((CarDecal)stickerDist(gen));
        car->SetTireType((CarWheelType)tireDist(gen));

    }

    #endif

}