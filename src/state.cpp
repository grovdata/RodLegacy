#include "log.h"
#include "state.h"
#include "scene.h"
#include "car.h"
#include "diner.h"
#include "garage/garage.h"
#include "gas_station.h"
#include "driving.h"
#include "newspaper.h"
#include "king_of_county.h"
#include "racing.h"
#include "debug_overlay.h"
#include "parsers/libp.h"
#include "parsers/adf.h"
#include "zip_utils.h"
#include <fstream>
#include <cstring>

State::State() : bankroll(5750) {
}

void State::Init(bool debugMode, const fs::path& gamePath, bool muteAudio) {
    this->debugMode = debugMode;
    try {
        assets.Init(gamePath, muteAudio);
        assets.ReadAssets();
        std::pair<int, std::vector<BitmapEntry_t>> entries = assets.GetImageEntries();   

        carData.ReadEntries(assets.GetSrData(), GetTomlFile(), entries);

        // Parse SR_data driver/car-spec tables and initialise pool.
        DataParser dataParser;
        srDriverData  = dataParser.ParseDriverData(assets.GetSrData());
        srCarSpecData = dataParser.ParseCarSpecTable(assets.GetSrData());
        if (srDriverData.size() == SR_DATA_DRIVER_ENTRY_COUNT &&
            srCarSpecData.size() == SR_DATA_CARSPEC_ENTRY_COUNT) {
            racerPool.Init(srDriverData, srCarSpecData);
            LOG_INFO("RacerPool initialised: %d active opponents", racerPool.CountActive());
        } else {
            LOG_INFO("SR_data parse incomplete (drivers=%zu carspecs=%zu) — pool disabled",
                     srDriverData.size(), srCarSpecData.size());
        }

        // Initialize roster
        roster.Initialize(carData);

        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_2DR_COUPE_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_ROADSTER_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_MASTER_DELUXE_2DR_1938)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1961)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_CORVETTE_1963)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(DODGE_CUSTOM_ROYAL_LANCER_1955)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_DEUCE_COUPE_1932)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(DODGE_POLARA_1962)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(PLYMOUTH_SAVOY_2DR_1962)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_SEDAN_1932)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_DELUXE_2DR_1940)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_FAIRLANE_VICTORIA_1955)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_FAIRLANE_500_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(MERCURY_MONTEREY_1954)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(MERCURY_CUSTOM_2DR_1956)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_STYLELINE_2DR_1949)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_IMPALA_1958)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(OLDSMOBILE_88_2DR_CUSTOM_1949)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(OLDSMOBILE_88_CUSTOM_1952)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(PONTIAC_SILVER_STREAK_1950)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(FORD_THUNDERBIRD_1957)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(VALIANT_V200_1961)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(VALIANT_V100_2DR_1963)));
        ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(CHEVROLET_BELAIR_SPORTS_COUPE)));

        // Generate classified cars for the newspaper
        for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
            int carAssetId = kCarIndex[i];
            CarMetadata_t metadata = carData.GetCarMetadata(carAssetId);
            if (metadata.classified.carName && strlen(metadata.classified.carName) > 0) {
                CarModel model = carData.GetModelFromIndex(carAssetId);
                Car* car = roster.GenerateRandomCar(model);
                // Stock appearance for classifieds
                car->SetRoofChopped(false);
                car->SetBumpersStripped(false);
                car->SetDecal(SrLibCarDecalNone);
                car->SetScoop(SCOOP_NONE);
                classifiedCars.push_back(car);
            }
        }
        LOG_INFO("Generated %zu classified cars", classifiedCars.size());

        // Seed spare tires for testing
        spareTires = { STOCK, BRAND, RACING };

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        LOG_INFO("Current car tires: %d", this->currentCar->GetTires().assetId);

        // Create scenes after fonts are initialized
        garage = std::make_unique<Garage>();
        diner = std::make_unique<Diner>();
        gasStation = std::make_unique<GasStation>();
        driving = std::make_unique<Driving>();
        newspaper = std::make_unique<Newspaper>();

        garage->Init(debugMode);
        diner->Init(debugMode);
        gasStation->Init(debugMode);
        newspaper->Init(debugMode);
        driving->Init(debugMode);
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        // Clean up any resources that were allocated before the exception
        delete currentCar;
        throw; // Re-throw to main
    }
}

State::~State() = default;

const std::string State::GetTomlFile() {
    return "../data/cars.toml";
}

int State::GetBankroll() {
    return bankroll;
}

void State::SetBankroll(int amount) {
    bankroll = amount;
}

SrLibGameScene State::GetCurrentGameScene() {
    return currentGameScene;
}

Scene* State::GetCurrentScene() {
    return currentScene;
}

void State::SetCurrentScene(Scene *scene) {
    this->currentScene = scene;
}

void State::MoveToScene(SrLibGameScene scene) {
    if (currentScene) {
        currentScene->OnSceneExit();
    }
    currentGameScene = scene;
    switch (scene) {
        case SCENE_DINER:
            this->currentScene = diner.get();
            break;
        case SCENE_GARAGE:
            this->currentScene = garage.get();
            break;
        case SCENE_GAS_STATION:
            this->currentScene = gasStation.get();
            break;
        case SCENE_CRUISING:
        case SCENE_DRIVING:
            this->currentScene = driving.get();
            break;
        case SCENE_NEWSPAPER:
            this->currentScene = newspaper.get();
            break;
        case SCENE_KING_OF_COUNTY:
            kingOfCounty = std::make_unique<KingOfCounty>(true);
            kingOfCounty->Init(debugMode);
            this->currentScene = kingOfCounty.get();
            break;
    }

    this->currentScene->FadeIn();
}

Car* State::GetCurrentCar() {
    return this->currentCar;
}

void State::SetCurrentCar(Car *car) {
    this->currentCar = car;
}

std::vector<Car *> State::GetOwnedCars() {
    return ownedCars;
}

void State::AddOwnedCar(Car* car) {
    ownedCars.push_back(car);
}

void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

std::string State::GetGameDateString() const {
    // Game starts June 15, 1963 at midnight; gameHours=12 = noon that day.
    int dayOffset = gameHours / 24;

    static const struct { const char* name; int days; } months[] = {
        { "June",      30 },
        { "July",      31 },
        { "August",    31 },
        { "September", 30 },
    };
    // Day 0 = June 15
    int day = 15 + dayOffset;
    int m = 0;
    while (m < 3 && day > months[m].days) {
        day -= months[m].days;
        m++;
    }
    return std::string(months[m].name) + " " + std::to_string(day) + ", 1963";
}

int State::CalculateScore() const {
    // DOS formula: score = elapsed_ticks/546 + 12, where 546 ticks = 1 hour
    // and the game starts at noon June 15 (hour 12).
    // We track gameHours starting at 12, so score == gameHours directly.
    return gameHours;
}

Opponent* State::GetNextOpponent() {
    return roster.GetNextOpponent();
}

Opponent* State::GetCurrentOpponent() {
    return roster.GetCurrentOpponent();
}

void State::AddKnownOpponent(Opponent* opp) {
    roster.AddKnownOpponent(opp);
}

const std::vector<Opponent*>& State::GetKnownOpponents() const {
    return roster.GetKnownOpponents();
}

void State::SetNextOpponent(Opponent* opp) {
    roster.SetNextOpponent(opp);
}

int State::GetPlayerScore() const {
    // Scored the same way an opponent's car spec is (see racer_pool.cpp), just
    // built from the player's installed parts rather than from a packed config
    // word. SrCarSpecEntry is not involved: it is a raw 14-byte SR_data record
    // and has no room for player state.
    srrace::CarSetup setup{};
    if (currentCar) {
        setup.engineClass   = static_cast<int>(currentCar->GetEngine().engineType);
        setup.transClass    = static_cast<int>(currentCar->GetGearbox().partType.transmissionType);
        setup.carbClass     = static_cast<int>(currentCar->GetCarburator().partType.carburetorType);
        setup.tireClass     = static_cast<int>(currentCar->GetWheelType());
    }
    return std::clamp(srrace::ComputeBasePerformance(setup), 0, 130);
}

int State::GetChallengerScore() const {
    if (lastChallengerPoolIndex < 0 ||
        lastChallengerPoolIndex >= RACER_POOL_SIZE) return 70;
    return racerPool.GetEntryScore(lastChallengerPoolIndex);
}

void State::PickNextOpponent() {
    // Fall back to Roster shuffle if pool is not initialised.
    if (racerPool.pool.empty()) {
        return;
    }

    int playerScore = GetPlayerScore();

    // Check for King unlock.
    bool kingUnlocked = reputation.IsKingUnlocked(racerPool.CountActive());
    if (kingUnlocked) {
        LOG_INFO("State::PickNextOpponent - The King is unlocked");
        // King challenge uses the existing roster's next opponent slot for now.
        // Full King flow wired in OnRaceFinished.
        lastChallengerPoolIndex = RACER_KING_SLOT;
        return;
    }

    int poolIdx = racerPool.SelectNextChallenger(playerScore, lastChallengerPoolIndex);
    if (poolIdx < 0) {
        LOG_INFO("State::PickNextOpponent - pool exhausted, falling back to roster");
        return;
    }
    lastChallengerPoolIndex = poolIdx;

    // Map pool entry → Roster Opponent so existing Diner code works unchanged.
    RacerEntry& entry = racerPool.pool[poolIdx];
    int driverIdx = std::min(entry.driver_index, (int)roster.GetOpponentCount() - 1);
    Opponent* opp = roster.GetOpponentByIndex(driverIdx);
    if (opp) {
        roster.SetNextOpponent(opp);
        LOG_INFO("State::PickNextOpponent - pool[%d] driver=%d car_spec=%d score=%d",
                 poolIdx, entry.driver_index, entry.car_spec_index, playerScore);
    }
}

bool State::OnRaceFinished(RaceType raceType, bool won, bool pink_slip) {
    if (won) {
        reputation.RecordWin(raceType);
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceWon(lastChallengerPoolIndex, pink_slip);
        }
        if (pendingIsPinkSlip) {
            // TODO: transfer opponent car to ownedCars
            LOG_INFO("State::OnRaceFinished - pink slip win, car transfer not yet implemented");
        } else {
            bankroll += pendingBetAmount;
        }
        LOG_INFO("State::OnRaceFinished - WIN $%d bankroll=%d drag_rep=%d road_rep=%d pool_active=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep,
                 racerPool.CountActive());

        // King beaten in drag race → end-game.
        if (lastChallengerPoolIndex == RACER_KING_SLOT && raceType == RaceType::Drag) {
            LOG_INFO("State::OnRaceFinished - King beaten in drag! Triggering end-game.");
            return true;
        }
    } else {
        reputation.RecordLoss();
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceLost(lastChallengerPoolIndex);
        }
        if (pendingIsPinkSlip) {
            // TODO: remove current car from ownedCars and transfer to opponent
            LOG_INFO("State::OnRaceFinished - pink slip loss, car transfer not yet implemented");
        } else {
            bankroll = std::max(0, bankroll - pendingBetAmount);
        }
        LOG_INFO("State::OnRaceFinished - LOSS $%d bankroll=%d drag_rep=%d road_rep=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep);
    }
    return false;
}

static constexpr int HALL_DAT_ENTRY_COUNT  = 10;
static constexpr int HALL_DAT_NAME_LEN     = 16;
static constexpr int HALL_DAT_SCORE_LEN    = 7;
static constexpr int HALL_DAT_ENTRY_SIZE   = HALL_DAT_NAME_LEN + HALL_DAT_SCORE_LEN; // 23
static constexpr int HALL_DAT_HEADER_SIZE  = 2;
static constexpr int HALL_DAT_FILE_SIZE    = HALL_DAT_HEADER_SIZE + HALL_DAT_ENTRY_COUNT * HALL_DAT_ENTRY_SIZE; // 232
static constexpr uint8_t HALL_DAT_XOR_KEY  = 0x80;

static std::vector<State::HallEntry> ParseHallDat(const uint8_t* data, size_t size) {
    std::vector<State::HallEntry> entries;
    if (size < (size_t)HALL_DAT_HEADER_SIZE) return entries;

    // First 2 bytes: big-endian int16 entry count (Amiga native, unencoded)
    int count = (int)((data[0] << 8) | data[1]);
    if (count <= 0 || count > HALL_DAT_ENTRY_COUNT) count = HALL_DAT_ENTRY_COUNT;

    // Only iterate as many complete entries as fit in the file
    int entries_that_fit = ((int)size - HALL_DAT_HEADER_SIZE) / HALL_DAT_ENTRY_SIZE;
    if (count > entries_that_fit) count = entries_that_fit;

    // Remaining bytes are XOR'd with 0x80
    for (int i = 0; i < count; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        char name[HALL_DAT_NAME_LEN + 1] = {};
        char score[HALL_DAT_SCORE_LEN + 1] = {};
        for (int b = 0; b < HALL_DAT_NAME_LEN; b++)
            name[b] = (char)(data[offset + b] ^ HALL_DAT_XOR_KEY);
        for (int b = 0; b < HALL_DAT_SCORE_LEN; b++)
            score[b] = (char)(data[offset + HALL_DAT_NAME_LEN + b] ^ HALL_DAT_XOR_KEY);
        if (name[0] == '\0') continue;
        State::HallEntry entry;
        entry.name  = name;
        entry.score = std::atoi(score);
        entries.push_back(entry);
    }
    return entries;
}

std::vector<State::HallEntry> State::LoadHallDat() const {
    // 1. Try local hall_dat file
    fs::path localPath = assets.GetBasePath() / "hall_dat";
    if (fs::exists(localPath)) {
        std::ifstream f(localPath, std::ios::binary);
        if (f) {
            std::vector<uint8_t> buf(HALL_DAT_FILE_SIZE, 0);
            f.read(reinterpret_cast<char*>(buf.data()), HALL_DAT_FILE_SIZE);
            auto entries = ParseHallDat(buf.data(), (size_t)f.gcount());
            LOG_INFO("LoadHallDat: loaded %zu entries from local hall_dat", entries.size());
            return entries;
        }
    }

    // 2. Fall back to ADF disk2 (direct file or inside sr1amiga.zip)
    const char* disk2Name = SrLibArchiveNames[SrLibArchiveSR1AmigaDisk2];
    fs::path disk2Path = assets.GetBasePath() / disk2Name;
    AdfFileData* adfData = nullptr;

    if (fs::exists(disk2Path)) {
        adfData = extract_adf_file_to_memory(disk2Path.string().c_str(), "hall_dat");
    } else {
        fs::path zipPath = assets.GetBasePath() / "sr1amiga.zip";
        if (fs::exists(zipPath)) {
            std::vector<uint8_t> disk2Buf = ZipExtractToMemory(zipPath, disk2Name);
            if (!disk2Buf.empty())
                adfData = extract_adf_file_from_buffer(disk2Buf.data(), disk2Buf.size(), "hall_dat");
        }
    }

    if (adfData) {
        auto entries = ParseHallDat(adfData->data, adfData->size);
        free_adf_file_data(adfData);
        LOG_INFO("LoadHallDat: loaded %zu entries from ADF disk2", entries.size());
        return entries;
    }

    LOG_INFO("LoadHallDat: hall_dat not found, starting with empty table");
    return {};
}

void State::SaveHallDat(const std::vector<HallEntry>& entries) const {
    uint8_t buf[HALL_DAT_FILE_SIZE] = {};

    // Header: int16 entry count (big-endian, Amiga native, unencoded)
    buf[0] = (HALL_DAT_ENTRY_COUNT >> 8) & 0xFF;
    buf[1] = HALL_DAT_ENTRY_COUNT & 0xFF;

    // Write up to 10 entries, XOR'd with 0x80
    int count = std::min((int)entries.size(), HALL_DAT_ENTRY_COUNT);
    for (int i = 0; i < count; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        // name[16]
        for (int b = 0; b < HALL_DAT_NAME_LEN; b++) {
            char c = (b < (int)entries[i].name.size()) ? entries[i].name[b] : '\0';
            buf[offset + b] = (uint8_t)(c ^ HALL_DAT_XOR_KEY);
        }
        // score[7] as decimal string
        char scoreBuf[HALL_DAT_SCORE_LEN + 1] = {};
        snprintf(scoreBuf, sizeof(scoreBuf), "%d", entries[i].score);
        for (int b = 0; b < HALL_DAT_SCORE_LEN; b++) {
            buf[offset + HALL_DAT_NAME_LEN + b] = (uint8_t)(scoreBuf[b] ^ HALL_DAT_XOR_KEY);
        }
    }
    // Remaining slots: XOR'd zero bytes = 0x80 for each byte
    for (int i = count; i < HALL_DAT_ENTRY_COUNT; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        for (int b = 0; b < HALL_DAT_ENTRY_SIZE; b++)
            buf[offset + b] = HALL_DAT_XOR_KEY;
    }

    fs::path localPath = assets.GetBasePath() / "hall_dat";
    std::ofstream f(localPath, std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(buf), HALL_DAT_FILE_SIZE);
        LOG_INFO("SaveHallDat: saved %d entries to %s", count, localPath.string().c_str());
    } else {
        LOG_INFO("SaveHallDat: failed to write %s", localPath.string().c_str());
    }
}

// Global object
State state;
