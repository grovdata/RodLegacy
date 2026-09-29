#include "core/log.h"
#include "race/roster.h"
#include "parsers/carcolor.h"
#include "core/state.h"
#include "car/car_builder.h"
#include "core/utils.h"
#include <random>
#include <algorithm>
#include <filesystem>

Opponent::Opponent() : car(nullptr) {}

Roster::Roster() : currentOpponent(opponents.begin()) {}

void Roster::LoadCustomDrivers() {
    customDriverPaths.clear();

    namespace fs = std::filesystem;
    const fs::path driverDir = state.GetAssets().GetBasePath() / "drivers";
    LOG_INFO("LoadCustomDrivers: looking in %s", driverDir.string().c_str());
    if (!fs::exists(driverDir) || !fs::is_directory(driverDir)) {
        LOG_INFO("LoadCustomDrivers: directory not found");
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
    LOG_INFO("Loaded %zu custom driver faces from drivers/", customDriverPaths.size());
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
    LOG_INFO("Loaded %zu custom sticker paths for roster", customStickerPaths.size());
}

void Roster::Initialize(CarData& carData) {
    LoadCustomDrivers();
    LoadCustomStickers();

    auto& gen = RandomEngine();

    // Any buyable car from any enabled set. The merged table's -1 entries are The King's slot in each set.
    std::vector<int> poolCarIndices;
    for (int assetId : state.GetCarSpecModelIndex()) {
        if (assetId >= 0) poolCarIndices.push_back(assetId);
    }
    if (poolCarIndices.empty()) {
        for (int i = 0; i < CAR_INDEX_COUNT; i++) poolCarIndices.push_back(kCarIndex[i]);
    }
    std::uniform_int_distribution<> distrCar(0, (int)poolCarIndices.size() - 1);

    opponents.clear();

    while (!stockDriverNames.empty()) {
        int preferredCarIndex = poolCarIndices[distrCar(gen)];

        Driver driver = GenerateRandomDriver(preferredCarIndex);
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
        CarPtr car = GenerateRandomCar(model);

        opponents.emplace_back();
        opponents.back().driver = std::move(driver);
        opponents.back().car = car;
    }

    for (const auto& path : customDriverPaths) {
        int preferredCarIndex = poolCarIndices[distrCar(gen)];
        CarModel model = carData.GetModelFromIndex(preferredCarIndex);
        CarPtr car = GenerateRandomCar(model);

        Driver driver;
        driver.customDriverPath = path;
        std::string stem = std::filesystem::path(path).stem().string();
        if (!stem.empty()) {
            std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
            stem[0] = std::toupper(stem[0]);
        }
        driver.name = stem;
        driver.preferredCar = preferredCarIndex;
        driver.spriteIndex = DRIVER_01;

        opponents.emplace_back();
        opponents.back().driver = std::move(driver);
        opponents.back().car = car;
    }

    ShuffleOpponents();
    currentOpponent = opponents.begin();

    LOG_INFO("--- Opponent roster (%zu) ---", opponents.size());
    for (size_t i = 0; i < opponents.size(); i++) {
        const auto& o = opponents[i];
        if (!o.driver.customDriverPath.empty()) {
            LOG_INFO("  %zu. %s (custom: %s)", i + 1, o.driver.name.c_str(), o.driver.customDriverPath.c_str());
        } else {
            LOG_INFO("  %zu. %s (stock sprite %d)", i + 1, o.driver.name.c_str(), o.driver.spriteIndex);
        }
    }
}

CarPtr Roster::GenerateRandomCar(const CarModel& model) {
    CarPtr car = CarBuilder::BuildRandomCar(model);
    
    auto& gen = RandomEngine();
    std::uniform_int_distribution<> colorDist(0, StandardCarColor::CarColorCount - 1);
    StandardCarColor randomColor = static_cast<StandardCarColor>(colorDist(gen));
    
    car->SetColor(randomColor);

    std::uniform_int_distribution<> roofDist(0, 1);
    bool isRoofChopped = roofDist(gen) == 1;
    car->SetRoofChopped(isRoofChopped);
    bool canChopRoof = CarModelCanChopRoof(model);
    if (!canChopRoof) {
        car->SetRoofChopped(false);
    }

    bool areBumpersStripped = false;
   
    bool canStripBumpers = CarModelCanStripBumpers(model);
    if (!canStripBumpers) {
        car->SetBumpersStripped(false);
    } else {
        std::uniform_int_distribution<> stripDist(0, 1);
        areBumpersStripped = stripDist(gen) == 1;
    }
    
    car->SetBumpersStripped(areBumpersStripped);
    // Only lib.p art is recoloured; DOS cars carry their own paint.
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::AmigaLibP) {
        state.GetAssets().UpdateAssetWithCarPalette(state.GetAssets().GetLibPData(), model.index, randomColor);
    }

    std::uniform_int_distribution<> stickerDist(0, (int)SrLibCarDecalNone - 1);

    // Wear stays 0, as in the original's generator.
    std::uniform_int_distribution<> chanceDist(0, 3);
    if (chanceDist(gen) == 0) {
        car->SetDecal(SrLibCarDecalNone);
    } else if (!customStickerPaths.empty()) {
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

    std::uniform_int_distribution<> tuningDist(0, kTuneLevelCount - 1);
    std::uniform_int_distribution<> optimalTuningDist(0, 3);
    if (optimalTuningDist(gen) == 0) {
        car->SetTuneLevel(kPerfectTuneLevel);
    } else {
        car->SetTuneLevel(tuningDist(gen));
    }

    // Drivetrain comes from the car's SR_data spec, not random. Must come last: SetManifold/SetCarburator
    // clear the spec classes.
    ApplyCarSpecFor(car.get(), model);

    return car;
}

// No row (a TOML-only custom car) leaves whatever CarBuilder put in.
void Roster::ApplyCarSpecFor(Car* car, const CarModel& model) {  // NOLINT

    const std::vector<SrCarSpecEntry>& specs = state.GetMergedCarSpecs();
    const std::vector<int>& specModel = state.GetCarSpecModelIndex();
    const size_t count = std::min(specs.size(), specModel.size());
    for (size_t i = 0; i < count; ++i) {
        if (specModel[i] == model.index) {
            CarBuilder::ApplySpec(car, specs[i], model.vendor);
            return;
        }
    }
}

void Roster::SetDriverTable(const std::vector<SrDriverData>& drivers,
                           const std::vector<std::string>& names) {
    srDrivers = drivers;
    srDriverNames = names;
    nextSrDriver = 0;
}

Driver Roster::GenerateRandomDriver(int preferredCarIndex) {
    auto& gen = RandomEngine();
    std::uniform_int_distribution<> spriteIndexDist(DRIVER_01, DRIVER_01 + DRIVER_COUNT - 1);
    Driver driver;
    // Name and portrait are a stored pair in SR_data; the portrait cannot be derived or picked at random.
    const bool haveSrDrivers =
        nextSrDriver < static_cast<int>(srDrivers.size()) &&
        nextSrDriver < static_cast<int>(srDriverNames.size()) &&
        nextSrDriver < SR_DATA_KING_DRIVER_INDEX;
    if (haveSrDrivers) {
        driver.srDriverIndex = nextSrDriver;
        driver.name = srDriverNames[nextSrDriver];
        driver.spriteIndex = srDrivers[nextSrDriver].PortraitAsset();
        ++nextSrDriver;
        if (!stockDriverNames.empty()) {
            stockDriverNames.pop_back();
        }
    } else {
        driver.name = stockDriverNames.back();
        if (driver.name == "Erica") {
            driver.spriteIndex = DRIVER_KINGS_GIRLFRIEND;
        } else {
            do {
                driver.spriteIndex = spriteIndexDist(gen);
            } while (driver.spriteIndex == DRIVER_KINGS_GIRLFRIEND);
        }
        stockDriverNames.pop_back();
    }
    driver.preferredCar = preferredCarIndex;

    if (!customDriverPaths.empty()) {
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
    if (forcedNextOpponent) {
        activeOpponent = forcedNextOpponent;
        forcedNextOpponent = nullptr;
        return activeOpponent;
    }
    if (currentOpponent == opponents.end()) {
        currentOpponent = opponents.begin();
    }
    activeOpponent = &*(currentOpponent++);
    return activeOpponent;
}

void Roster::AddKnownOpponent(Opponent* opp) {
    for (const auto* known : knownOpponents) {
        if (known->driver.name == opp->driver.name) {
            return;
        }
    }
    knownOpponents.push_back(opp);
}

const std::vector<Opponent*>& Roster::GetKnownOpponents() const {
    return knownOpponents;
}

Opponent* Roster::FindOpponentByName(const std::string& name) {
    for (auto& opp : opponents) {
        if (opp.driver.name == name) {
            return &opp;
        }
    }
    return nullptr;
}

void Roster::SetNextOpponent(Opponent* opp) {
    forcedNextOpponent = opp;
}

bool Roster::HasMoreOpponents() const {
    return !opponents.empty();
}

void Roster::ShuffleOpponents() {
    auto& gen = RandomEngine();
    std::shuffle(opponents.begin(), opponents.end(), gen);
    currentOpponent = opponents.begin();
}

Opponent* Roster::GetCurrentOpponent() {
    // currentOpponent is post-incremented past them, and a forced pick never moves it.
    return activeOpponent;
}

Opponent* Roster::GetOpponentByIndex(int index) {
    if (index < 0 || index >= static_cast<int>(opponents.size())) return nullptr;
    return &opponents[index];
}

Opponent* Roster::GetKingOpponent(CarData& carData, const SrCarSpecEntry& kingSpec) {
    // Rebuild if his car was taken in a pink-slip race.
    if (kingOpponentBuilt && kingOpponent.car) return &kingOpponent;

    if (SR_DATA_KING_DRIVER_INDEX < static_cast<int>(srDrivers.size()) &&
        SR_DATA_KING_DRIVER_INDEX < static_cast<int>(srDriverNames.size())) {
        kingOpponent.driver.srDriverIndex = SR_DATA_KING_DRIVER_INDEX;
        kingOpponent.driver.name = srDriverNames[SR_DATA_KING_DRIVER_INDEX];
        kingOpponent.driver.spriteIndex = srDrivers[SR_DATA_KING_DRIVER_INDEX].PortraitAsset();
    } else {
        kingOpponent.driver.name = "The King";
        kingOpponent.driver.spriteIndex = DRIVER_01;
    }
    kingOpponent.driver.preferredCar = CHEVROLET_CORVETTE_1963;

    // ponytail: the King's body id has no CarData metadata, so build a '63 Corvette and swap in his paint.
    // Its spec bytes match his, so this is his real car. Upgrade if he ever needs his own metadata row.
    CarModel model = carData.GetModelFromIndex(CHEVROLET_CORVETTE_1963);
    model.sprites.body = THE_KING_CHEVROLET_CORVETTE_1963_BODY;
    // His body art already has the scoop.
    model.sprites.airScoop = -1;
    CarPtr car = CarBuilder::BuildCar(model);
    car->SetRoofChopped(false);
    car->SetBumpersStripped(false);
    car->SetDecal(SrLibCarDecalNone);
    // Paint 13; no other car ever gets it.
    car->SetColor(CarColorKing);
    CarBuilder::ApplySpec(car.get(), kingSpec, model.vendor);
    kingOpponent.car = car;

    kingOpponentBuilt = true;
    return &kingOpponent;
}