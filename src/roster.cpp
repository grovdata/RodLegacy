#include "roster.h"
#include "parsers/carcolor.h"
#include "state.h"
#include "car_builder.h"
#include <random>
#include <algorithm>
#include <filesystem>

Opponent::Opponent() : car(nullptr) {}

Roster::Roster() : currentOpponent(opponents.begin()) {}

void Roster::LoadCustomDrivers() {
    customDriverPaths.clear();

    namespace fs = std::filesystem;
    const fs::path driverDir = state.GetAssets().GetBasePath() / "drivers";
    SDL_Log("LoadCustomDrivers: looking in %s", driverDir.string().c_str());
    if (!fs::exists(driverDir) || !fs::is_directory(driverDir)) {
        SDL_Log("LoadCustomDrivers: directory not found");
        return;
    }

    std::vector<fs::path> pngFiles;
    for (const auto& entry : fs::directory_iterator(driverDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".PNG") {
                pngFiles.push_back(entry.path());
            }
        }
    }
    std::sort(pngFiles.begin(), pngFiles.end());

    for (const auto& p : pngFiles) {
        customDriverPaths.push_back(p.string());
    }
    SDL_Log("Loaded %zu custom driver faces from drivers/", customDriverPaths.size());
}

void Roster::LoadCustomStickers() {
    customStickerPaths.clear();

    namespace fs = std::filesystem;
    const fs::path stickerDir = state.GetAssets().GetBasePath() / "stickers";
    if (!fs::exists(stickerDir) || !fs::is_directory(stickerDir)) {
        return;
    }

    std::vector<fs::path> pngFiles;
    for (const auto& entry : fs::directory_iterator(stickerDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".PNG") {
                pngFiles.push_back(entry.path());
            }
        }
    }
    std::sort(pngFiles.begin(), pngFiles.end());

    for (const auto& p : pngFiles) {
        customStickerPaths.push_back(p.string());
    }
    SDL_Log("Loaded %zu custom sticker paths for roster", customStickerPaths.size());
}

void Roster::Initialize(CarData& carData) {
    LoadCustomDrivers();
    LoadCustomStickers();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrCar(0, CAR_INDEX_COUNT - 1);

    opponents.clear();

    while (!stockDriverNames.empty()) {
        int preferredCarIndex = CAR_INDEX[distrCar(gen)];

        Driver driver = GenerateRandomDriver(preferredCarIndex);
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
        Car* car = GenerateRandomCar(model);

        opponents.emplace_back();
        opponents.back().driver = std::move(driver);
        opponents.back().car = car;
    }

    // Add one opponent per custom driver
    for (const auto& path : customDriverPaths) {
        int preferredCarIndex = CAR_INDEX[distrCar(gen)];
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
        Car* car = GenerateRandomCar(model);

        Driver driver;
        driver.customDriverPath = path;
        std::string stem = std::filesystem::path(path).stem().string();
        if (!stem.empty()) {
            std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
            stem[0] = std::toupper(stem[0]);
        }
        driver.name = stem;
        driver.preferredCar = preferredCarIndex;
        driver.spriteIndex = DRIVER_01; // fallback

        opponents.emplace_back();
        opponents.back().driver = std::move(driver);
        opponents.back().car = car;
    }

    ShuffleOpponents();
    currentOpponent = opponents.begin();

    SDL_Log("--- Opponent roster (%zu) ---", opponents.size());
    for (size_t i = 0; i < opponents.size(); i++) {
        const auto& o = opponents[i];
        if (!o.driver.customDriverPath.empty()) {
            SDL_Log("  %zu. %s (custom: %s)", i + 1, o.driver.name.c_str(), o.driver.customDriverPath.c_str());
        } else {
            SDL_Log("  %zu. %s (stock sprite %d)", i + 1, o.driver.name.c_str(), o.driver.spriteIndex);
        }
    }
}

Car* Roster::GenerateRandomCar(const CarModel& model) {
    Car* car = CarBuilder::BuildRandomCar(model);
    
    // Random color selection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> colorDist(0, StandardCarColor::CarColorCount - 1);
    StandardCarColor randomColor = static_cast<StandardCarColor>(colorDist(gen));
    
    // Set the car color
    car->SetColor(randomColor);

    // Randomize roof chopping
    std::uniform_int_distribution<> roofDist(0, 1);
    bool isRoofChopped = roofDist(gen) == 1;
    car->SetRoofChopped(isRoofChopped);
    bool canChopRoof = CarQuery::SrLibQueryCanChopRoof((AmigaLibAsset)model.index);
    if (!canChopRoof) {
        car->SetRoofChopped(false);
    }

    bool areBumpersStripped = false;
   
    bool canStripBumpers = CarQuery::SrLibQueryCanStripBumpers((AmigaLibAsset)model.index);
    if (!canStripBumpers) {
        car->SetBumpersStripped(false);
    } else {
        std::uniform_int_distribution<> stripDist(0, 1);
        areBumpersStripped = stripDist(gen) == 1;
    }
    
    car->SetBumpersStripped(areBumpersStripped);
    // Update the asset with the selected color
    state.GetAssets().UpdateAssetWithCarPalette(state.GetAssets().GetLibPData(), model.index, randomColor); 

    std::uniform_int_distribution<> wearDist(0, 30);
    std::uniform_int_distribution<> stickerDist(0, (int)SrLibCarDecalNone - 1);
    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);
    std::uniform_int_distribution<> scoopDist(0, (int)CarScoopType::SCOOP_NONE - 1);

    // Set random customization and wear
    // Introduce a 25% chance to set CarDecal to CarDecalNone
    std::uniform_int_distribution<> chanceDist(0, 3); // 0 to 3 for 25% chance
    if (chanceDist(gen) == 0) {
        car->SetDecal(SrLibCarDecalNone);
    } else if (!customStickerPaths.empty()) {
        // Pool: 8 stock stickers + N custom stickers
        int totalStickers = (int)SrLibCarDecalNone + (int)customStickerPaths.size();
        std::uniform_int_distribution<> poolDist(0, totalStickers - 1);
        int pick = poolDist(gen);
        if (pick < (int)SrLibCarDecalNone) {
            car->SetDecal((SrLibCarDecal)pick);
        } else {
            car->SetDecal(SrLibCarDecalCustom);
            car->SetCustomDecalPath(customStickerPaths[pick - (int)SrLibCarDecalNone]);
        }
    } else {
        car->SetDecal((SrLibCarDecal)stickerDist(gen));
    }

    // Tuning level is divided into 13 steps
    std::uniform_int_distribution<> tuningDist(0, 12);
    std::uniform_int_distribution<> optimalTuningDist(0, 3);
    if (optimalTuningDist(gen) == 0) {
        // 9 is the optimal tuning
        car->SetTuneLevel(9);
    } else {
        car->SetTuneLevel(tuningDist(gen));
    }

    car->SetTires((CarWheelType)tireDist(gen));
    car->SetScoop((CarScoopType)scoopDist(gen));
    // Introduce a 50% chance to not have a scoop at all
    if (chanceDist(gen) == 0) {
        car->SetScoop(CarScoopType::SCOOP_NONE);
    } else {
        car->SetScoop((CarScoopType)scoopDist(gen));
    }
    // Randomize drivetrain and engine parts
    // Wear 0-30% similar to other parts
    int wearManifold = wearDist(gen);
    int wearCarb = wearDist(gen);

    // Vendor for this model
    CarVendor_t vendor = (CarVendor_t)CAR_VENDOR_LOOKUP[model.index][1];

    // Transmission is already set by CarBuilder with vendor-appropriate logic

    // Random manifold among vendor-sensible choices (map 1..6 to vendor)
    // For now, assume all manifolds fit vendor engines; keep asset range
    std::uniform_int_distribution<> maniDist(ENGINE_MANIFOLD1, ENGINE_MANIFOLD6);
    Part manifoldPart(wearManifold, 0, vendor, static_cast<AmigaLibAsset>(maniDist(gen)), MANIFOLD_STOCK);
    car->SetManifold(manifoldPart);

    // Random carb (2-barrel / 4-barrel / racing 4-barrel) - all vendor variants are universal in assets
    std::uniform_int_distribution<> carbPick(0, 2);
    AmigaLibAsset carbAsset = ENGINE_CARB_2BARREL;
    switch (carbPick(gen)) {
        case 0: carbAsset = ENGINE_CARB_2BARREL; break;
        case 1: carbAsset = ENGINE_CARB_4BARREL; break;
        case 2: carbAsset = ENGINE_CARB_4BARREL_RACING; break;
    }
    Part carbPart(wearCarb, 0, vendor, carbAsset, CARB_2BARREL);
    car->SetCarburator(carbPart);

    // Engine is already set by CarBuilder::BuildRandomCar(), no need to override it

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

    if (!customDriverPaths.empty()) {
        // Pool: DRIVER_COUNT stock sprites + N custom PNGs
        int totalChoices = DRIVER_COUNT + (int)customDriverPaths.size();
        std::uniform_int_distribution<> poolDist(0, totalChoices - 1);
        int pick = poolDist(gen);
        if (pick >= DRIVER_COUNT) {
            driver.customDriverPath = customDriverPaths[pick - DRIVER_COUNT];
            // Derive name from filename: "zak.png" -> "Zak", "MARIO.PNG" -> "Mario"
            std::string stem = std::filesystem::path(driver.customDriverPath).stem().string();
            if (!stem.empty()) {
                std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
                stem[0] = std::toupper(stem[0]);
                driver.name = stem;
            }
        }
    }

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