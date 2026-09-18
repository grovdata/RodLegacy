#include "core/log.h"
#include "core/state.h"
#include "scenes/scene.h"
#include "car/car.h"
#include "scenes/diner.h"
#include "garage/garage.h"
#include "scenes/gas_station.h"
#include "race/driving.h"
#include "scenes/newspaper.h"
#include "race/king_of_county.h"
#include "scenes/name_entry.h"
#include "scenes/save_load_scene.h"
#include "scenes/game_over_menu.h"
#include "scenes/ending.h"
#ifdef RODLEGACY_DEBUG_UI
#include "debug/car_debug.h"
#endif
#include "race/racing.h"
#include "debug/debug_overlay.h"
#include "parsers/libp.h"
#include "car/car_builder.h"
#include "core/save_loader.h"
#include "race/bet_acceptance.h"
#include "core/utils.h"
#include <fstream>
#include <cstring>
#include <random>

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
        LoadBetTables(assets.GetSrData());

        RebuildCarSets();

        // Give the roster the real driver table before it builds opponents, so
        // names and portraits come from the data rather than a random sprite.
        roster.SetDriverTable(srDriverData, dataParser.ParseDriverNames(assets.GetSrData()));

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

        GenerateClassifieds();

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        LOG_INFO("Current car tires: %d", this->currentCar->GetTires().assetId);

        // Create scenes after fonts are initialized
        garage = std::make_unique<Garage>();
        diner = std::make_unique<Diner>();
        gasStation = std::make_unique<GasStation>();
        driving = std::make_unique<Driving>();
        newspaper = std::make_unique<Newspaper>();
        nameEntry = std::make_unique<NameEntry>();
        saveLoad = std::make_unique<SaveLoadScene>();
        gameOverMenu = std::make_unique<GameOverMenu>();
        ending = std::make_unique<Ending>();
#ifdef RODLEGACY_DEBUG_UI
        carDebug = std::make_unique<CarDebug>();
#endif

        garage->Init(debugMode);
        diner->Init(debugMode);
        gasStation->Init(debugMode);
        newspaper->Init(debugMode);
        driving->Init(debugMode);
        nameEntry->Init(debugMode);
        saveLoad->Init(debugMode);
        gameOverMenu->Init(debugMode);
        ending->Init(debugMode);
#ifdef RODLEGACY_DEBUG_UI
        carDebug->Init(debugMode);
#endif
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
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

bool State::TakePendingCruise(SrLibGameScene& destination) {
    if (!pendingCruise) {
        return false;
    }
    pendingCruise = false;
    destination = pendingCruiseDestination;
    return true;
}

void State::ArriveFromCruise(SrLibGameScene destination) {
    arrivingFromCruise = true;
    MoveToScene(destination);
}

void State::MoveToScene(SrLibGameScene scene) {
    const SrLibGameScene previousScene = currentGameScene;
    // The second half of a drive: the fuel and the cutscene were both dealt
    // with on the way in, so this leg is neither charged nor driven again.
    const bool fromCruise = arrivingFromCruise;
    arrivingFromCruise = false;

    // Every leg between two places you drive to costs fuel; the garage's
    // newspaper, save/load and the rest are not drives.
    auto isDriveTo = [](SrLibGameScene s) {
        return s == SCENE_GARAGE || s == SCENE_DINER || s == SCENE_GAS_STATION ||
               s == SCENE_DRIVING || s == SCENE_CRUISING;
    };
    if (!fromCruise && scene != previousScene && isDriveTo(previousScene) && isDriveTo(scene)) {
        BurnFuel(FUEL_PER_TRIP_TENTHS);
    }

    // Driving between two of the three places is the cruise cutscene. Driving
    // starts it from its own OnSceneEnter, which runs at the bottom of this
    // function - starting it here would be undone by that reset.
    auto isPlace = [](SrLibGameScene s) {
        return s == SCENE_GARAGE || s == SCENE_DINER || s == SCENE_GAS_STATION;
    };
    if (!fromCruise && driving && scene != previousScene && isPlace(previousScene) &&
        isPlace(scene)) {
        pendingCruise = true;
        pendingCruiseDestination = scene;
        scene = SCENE_CRUISING;
    }
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
            if (newspaper) newspaper->ResetToFrontPage();
            this->currentScene = newspaper.get();
            break;
        case SCENE_KING_OF_COUNTY:
            // Jail gets the same hall of fame, but you did not win it.
            kingOfCounty = std::make_unique<KingOfCounty>(!jailed);
            kingOfCounty->Init(debugMode);
            this->currentScene = kingOfCounty.get();
            break;
        case SCENE_NAME_ENTRY:
            this->currentScene = nameEntry.get();
            break;
        case SCENE_SAVE_LOAD:
            this->currentScene = saveLoad.get();
            break;
        case SCENE_GAME_OVER_MENU:
            this->currentScene = gameOverMenu.get();
            break;
        case SCENE_ENDING:
            this->currentScene = ending.get();
            break;
        case SCENE_CAR_DEBUG:
#ifdef RODLEGACY_DEBUG_UI
            this->currentScene = carDebug.get();
#else
            // Not built; stay where we are rather than null the scene.
            LOG_INFO("SCENE_CAR_DEBUG is only available in debug_ui builds");
            currentGameScene = previousScene;
            return;
#endif
            break;
    }

    // Before FadeIn, so the scene is reset before it is drawn once: the fade
    // does not hide the previous visit's leftovers, it fades them up.
    this->currentScene->OnSceneEnter();
    this->currentScene->FadeIn();
}

// Merges the car spec tables of every enabled set and re-seeds the racer pool
// from the result. Init calls it once; starting a new game calls it again,
// because the set the player picked on the way in changes the table - and the
// table's layout is what saved model indices are resolved against.
void State::RebuildCarSets() {
    DataParser dataParser;

    // The Amiga set always goes in first, so its specs keep indices 0..25
    // - the range original .SAV files store - whatever else is switched on.
    mergedCarSpecs = srCarSpecData;
    mergedCarSpecModelIndex.assign(srCarSpecData.size(), -1);
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT && i < (int)srCarSpecData.size(); i++) {
        mergedCarSpecModelIndex[i] = kCarIndex[i];
    }

    // Then each optional set the player switched on. Everything a set
    // needs - stats, sprite numbers, pixel offsets - comes out of its own
    // data file; see RodLegacyResearch/docs/formats/hot_data_dos.md.
    const std::vector<CarSetDef>& carSetDefs = GetCarSetDefs();
    for (int set = 0; set < (int)carSetDefs.size(); set++) {
        if (carSetDefs[set].mandatory || !assets.IsCarSetEnabled(set)) continue;

        const byte_vector& setData = assets.GetCarSetData(set);
        const int added = carData.ReadCarSet(set, setData, assets.GetCarSetImageEntries(set));
        if (added <= 0) continue;

        const std::vector<SrCarSpecEntry> specs = dataParser.ParseCarSpecTableDOS(setData);
        const std::vector<DosCarSprites> sprites = dataParser.ParseCarSpritesDOS(setData);
        for (int i = 0; i < (int)specs.size(); i++) {
            mergedCarSpecs.push_back(specs[i]);
            // The King's slot in each set has no buyable car behind it.
            const bool buyable = i < CAR_PLAYER_MODEL_COUNT && i < (int)sprites.size();
            mergedCarSpecModelIndex.push_back(
                buyable ? set * HOT_DATA_SPRITE_BASE + sprites[i].body : -1);
        }
        LOG_INFO("Car set '%s': %d cars, %zu specs", carSetDefs[set].id.c_str(), added, specs.size());
    }
    LOG_INFO("Merged car spec table: %zu entries across all enabled sets", mergedCarSpecs.size());

    if (srDriverData.size() == SR_DATA_DRIVER_ENTRY_COUNT &&
        srCarSpecData.size() == SR_DATA_CARSPEC_ENTRY_COUNT) {
        // Which cars can wear which modifications, so the pool only
        // rolls appearance changes the model actually has sprites for -
        // the same gate the original applies.
        std::vector<uint8_t> specMods(mergedCarSpecs.size(), 0);
        for (size_t i = 0; i < mergedCarSpecs.size(); ++i) {
            if (i >= mergedCarSpecModelIndex.size()) break;
            const int modelIdx = mergedCarSpecModelIndex[i];
            if (modelIdx < 0) continue;  // The King's slot, no buyable car
            const CarModel model = carData.GetModelFromIndex(modelIdx);
            if (CarModelCanChopRoof(model))     specMods[i] |= SPEC_CAN_CHOP_ROOF;
            if (CarModelCanStripBumpers(model)) specMods[i] |= SPEC_CAN_STRIP_BUMPERS;
        }
        racerPool.Init(mergedCarSpecs, specMods);
        LOG_INFO("RacerPool initialised: %d active opponents", racerPool.CountActive());
    } else {
        LOG_INFO("SR_data parse incomplete (drivers=%zu carspecs=%zu) — pool disabled",
                 srDriverData.size(), srCarSpecData.size());
    }
}

// Fills the newspaper's used-car ads
void State::GenerateClassifieds() {
    classifiedCars.clear();
    ++classifiedsGeneration;

    // Generate classified cars for the newspaper: every buyable car from
    // every enabled set, in merged-table order. Newspaper::LoadClassifieds
    // walks the same table with the same skip condition, because the buy
    // handler indexes classifiedCars by the newspaper's list position.
    for (int spec = 0; spec < static_cast<int>(mergedCarSpecs.size()); spec++) {
        const int carAssetId = mergedCarSpecModelIndex[spec];
        if (carAssetId < 0) continue;  // The King's slot, not for sale
        CarMetadata_t metadata = carData.GetCarMetadata(carAssetId);
        if (!metadata.classified.carName || strlen(metadata.classified.carName) == 0) {
            continue;
        }
        CarModel model = carData.GetModelFromIndex(carAssetId);
        CarPtr car = roster.GenerateRandomCar(model);
        // Stock appearance for classifieds...
        car->SetRoofChopped(false);
        car->SetBumpersStripped(false);
        car->SetDecal(SrLibCarDecalNone);
        // ...and the drivetrain the car is actually advertised with.
        CarBuilder::ApplySpec(car.get(), mergedCarSpecs[spec], model.vendor);
        classifiedCars.push_back(car);
    }
    LOG_INFO("Generated %zu classified cars", classifiedCars.size());
}

void State::StartNewGame() {
    // The car sets chosen for this game decide the spec table, so rebuild
    // before anything generates a car against it.
    RebuildCarSets();
    GenerateClassifieds();
    ownedCars.clear();
    currentCar.reset();
    jailed = false;
    kingQualifierWon = false;
    // Matches TrackGenerator(100) in maybeInitSomeGameState: one roll per
    // profile, then fixed for its life.
    SetTrackVariant(std::uniform_int_distribution<int>(0, 9)(RandomEngine()));
}

Car* State::GetCurrentCar() {
    return this->currentCar.get();
}

void State::BurnFuel(int tenths) {
    Car* car = GetCurrentCar();
    if (!car) return;
    car->SetFuelTenths(std::max(0, car->GetFuelTenths() - tenths));
}

void State::SetCurrentCar(CarPtr car) {
    this->currentCar = std::move(car);
}

void State::AddOwnedCar(CarPtr car) {
    ownedCars.push_back(std::move(car));
}

void State::RemoveOwnedCar(const CarPtr& car) {
    auto it = std::find(ownedCars.begin(), ownedCars.end(), car);
    if (it != ownedCars.end()) {
        ownedCars.erase(it);
    }
}

// $400 is the cheapest car in SR_data (the 1940 Chevrolet coupe), and the
// original hardcodes the same number.
bool State::IsOutOfDough() const {
    return currentCar == nullptr && ownedCars.empty() &&
           spareTires.empty() && spareTransmissions.empty() && spareManifolds.empty() &&
           spareCarburetors.empty() && spareEngines.empty() &&
           bankroll < 400;
}

void State::ScrapCurrentCar() {
    // The car stays alive as long as anything is still showing it (a cached
    // CarBitmap, a scene mid-animation) and is freed once they let go.
    CarPtr gone = currentCar;
    if (!gone) return;
    RemoveOwnedCar(gone);
    currentCar = ownedCars.empty() ? CarPtr{} : ownedCars.front();
    LOG_INFO("State::ScrapCurrentCar - lost %s, %zu car(s) left",
             gone->GetModel().name.c_str(), ownedCars.size());
}

int State::GetCarValue(const Car* car) {
    return car == nullptr ? 0 : car->GetValue();
}

int State::GetCarStrippedValue(const Car* car) {
    return car == nullptr ? 0 : car->GetStrippedValue();
}

namespace {

// Builds one Car* from a parsed car record. Shared by the active car and
// every roster car in State::LoadSaveGame.
//
// rec.specIndex is the confirmed save-format index (0-25, sr_data.md's
// 26-entry car table). CarData::GetModelFromIndex does NOT take that index
// directly -- CarModel.index is set from kCarIndex[i] (libp.h), a separate
// lib.p bitmap-asset ID, not the plain spec position. kCarIndex[] is
// positionally ordered to match sr_data.md exactly (confirmed: index 0 =
// CHEVROLET_2DR_COUPE_1940 ... 24 = CHEVROLET_BELAIR_SPORTS_COUPE), but
// only holds the 25 player-ownable cars -- The King (spec index 25) isn't
// in it, since his car is never actually owned by the player in practice.
CarPtr BuildCarFromRecord(const LoadedCarRecord& rec, CarData& carData,
                        const std::vector<SrCarSpecEntry>& specs) {
    if (rec.specIndex < 0 || rec.specIndex >= CAR_INDEX_COUNT) {
        LOG_INFO("LoadSaveGame: car spec index %d has no known lib.p asset (King's car or out of range), using a placeholder model", rec.specIndex);
    }
    int assetId = (rec.specIndex >= 0 && rec.specIndex < CAR_INDEX_COUNT) ? kCarIndex[rec.specIndex] : kCarIndex[0];
    CarModel model = carData.GetModelFromIndex(assetId);
    CarPtr car = CarBuilder::BuildCar(model);

    // The saved car keeps its own parts (loaded below), but the class byte is
    // a property of the model, not of the save, so it comes from the spec.
    if (rec.specIndex >= 0 && rec.specIndex < static_cast<int>(specs.size())) {
        car->SetWeightClass(specs[rec.specIndex].car_class);
    }

    car->SetColor(static_cast<StandardCarColor>(rec.colorIndex));
    // Shared decoders (libp.h). The sticker nibble is one-based, so reading it
    // as a plain index used to give every unstickered car decal 0 - which is
    // most of them, since the original leaves the nibble at 0 by default.
    car->SetRoofChopped(CarBodyRoofChopped(rec.bodyByte));
    car->SetBumpersStripped(CarBodyBumpersStripped(rec.bodyByte));
    car->SetDecal(CarBodyDecal(rec.bodyByte));
    // Confirmed save byte is signed -8..+4, 0=perfectly tuned. RodLegacy's
    // own tuneLevel is the same knob stored unsigned, perfect at
    // kPerfectTuneLevel - so the save byte maps onto it directly and the
    // full -8..+4 range lands inside 0..12 with nothing clamped off.
    car->SetTuneLevel(kPerfectTuneLevel + rec.tuningKnob);
    car->SetFuelTenths(rec.fuelTenths);

    // Sub-list slot -> category, confirmed in save_game.md.
    for (int slot = 0; slot < 7; ++slot) {
        if (rec.sublists[slot].empty()) continue;
        const LoadedPartRecord& p = rec.sublists[slot][0];
        switch (slot) {
            case 0: { // Transmission - decode owned by CarBuilder::TransmissionTypeFromSaveVariant
                TransmissionType type;
                if (!CarBuilder::TransmissionTypeFromSaveVariant(p.variant, type)) type = TRANS_4SPD_UNIVERSAL;
                Part part = CarBuilder::CreateTransmissionPart(type, model.vendor);
                part.wear = p.wearRaw;
                part.cost = p.value;
                car->SetGearbox(part);
                break;
            }
            case 1: { // Engine - decode owned by CarBuilder::EngineTypeFromSaveVariant
                CarEngine_t type;
                AmigaLibAsset asset;
                if (CarBuilder::EngineTypeFromSaveVariant(p.variant, type, asset)) {
                    EnginePart enginePart(type, p.wearRaw, p.value, model.vendor, asset);
                    car->SetEngine(enginePart);
                } else {
                    Part enginePart(p.wearRaw, p.value, model.vendor, ENGINE_V8, TRANS_4SPD_UNIVERSAL);
                    car->SetEngine(enginePart); // EnginePart::FromPart picks a vendor-appropriate type
                }
                break;
            }
            case 2: { // Manifold - decode owned by CarBuilder::ManifoldTypeFromSaveVariant
                ManifoldType type;
                if (!CarBuilder::ManifoldTypeFromSaveVariant(p.variant, type)) type = MANIFOLD_2BARREL;
                Part part = CarBuilder::CreateManifoldPart(type, model.vendor);
                // wearRaw is the fixed sentinel 0xFF80 here, not a real wear
                // value - manifolds don't track wear (save_game.md:100).
                part.cost = p.value;
                car->SetManifold(part);
                break;
            }
            case 3:    // Carb, and slots 4/5 its second and third - a
            case 4:    // dual-quad or triple-2bbl manifold really does carry
            case 5: {  // that many separate carburetors.
                CarburetorType type;
                if (!CarBuilder::CarburetorTypeFromSaveVariant(p.variant, type)) type = CARB_4BARREL;
                Part part = CarBuilder::CreateCarburetorPart(type, model.vendor);
                // wearRaw is the fixed sentinel 0xFF80 here, not a real wear
                // value - carbs don't track wear (save_game.md:100).
                part.cost = p.value;
                car->SetCarburetor(slot - 3, part);
                break;
            }
            case 6: { // Tires - decode owned by CarBuilder::TireTypeFromSaveVariant
                CarWheelType type;
                if (!CarBuilder::TireTypeFromSaveVariant(p.variant, type)) type = STOCK;
                car->SetTires(type, p.wearRaw, p.value);
                break;
            }
            default:
                break;
        }
    }
    // Last, against the parts just fitted: the save carries the original's own
    // live value word (save_game.md, car record 0x00).
    car->SetValue(rec.value);
    return car;
}

} // namespace

bool State::LoadSaveGame(const fs::path& path) {
    LoadedSave loaded;
    try {
        loaded = SaveGameLoader::Load(path);
    } catch (const std::exception& e) {
        LOG_INFO("LoadSaveGame failed: %s", e.what());
        return false;
    }

    SetBankroll(loaded.bankroll);
    SetElapsedTicks(loaded.elapsedTicks);
    SetTrackVariant(loaded.trackVariant);

    // Replace the owned-car roster. Anything still showing one of the old
    // cars (a cached CarBitmap, a scene mid-animation) keeps it alive until
    // it lets go, so dropping them here cannot dangle.
    ownedCars.clear();
    currentCar.reset();

    if (loaded.hasActiveCar) {
        CarPtr active = BuildCarFromRecord(loaded.activeCar, carData, srCarSpecData);
        ownedCars.push_back(active);
        currentCar = active;
    }
    for (const auto& rec : loaded.roster) {
        ownedCars.push_back(BuildCarFromRecord(rec, carData, srCarSpecData));
    }
    if (!currentCar && !ownedCars.empty()) currentCar = ownedCars.at(0);

    // Known opponents (trailer chunk 6): slot 21 is The King, who has no
    // mutable storage in RacerPool (RACER_KING_SLOT is a sentinel, not a
    // pool[] entry) -- only slots 0-20 are loaded.
    for (int i = 0; i < RACER_POOL_SIZE; ++i) {
        racerPool.SetCarSpecForSlot(i, loaded.opponentCarSpec[i]);
        if (i < static_cast<int>(racerPool.pool.size())) racerPool.pool[i].status = 0;
    }

    // Race record. The save stores standing directly (trailer chunk 5 - see
    // save_loader.h), so both reputations round-trip exactly.
    //
    // It does NOT store win counts: the original tracks only reputation and
    // races completed. drag_wins/road_wins are a RodLegacy addition, so the
    // best that can be done is leave them at 0 - reputation, which is what
    // actually gates opponent difficulty and the King, is exact.
    reputation = Reputation();
    reputation.races_completed = loaded.racesCompleted;
    reputation.drag_rep        = loaded.dragRep;
    reputation.road_rep        = loaded.roadRep;
    reputation.last_race_won   = loaded.lastRaceWon;

    // The header carries the profile name, space-padded (save_game.md).
    std::string savedName = loaded.name;
    while (!savedName.empty() && savedName.back() == ' ') savedName.pop_back();
    if (!savedName.empty()) SetPlayerName(savedName);

    LOG_INFO("LoadSaveGame: loaded '%s', bankroll=%d, %d owned car(s), %zu spare part(s) not sorted by category (see save_loader.h)",
              loaded.name.c_str(), loaded.bankroll, (int)ownedCars.size(), loaded.spareParts.size());
    return true;
}


void State::SetElapsedTime(Uint64 elapsedTime) {
    this->elapsedTime = elapsedTime;
}

Uint64 State::GetElapsedTime() {
    return this->elapsedTime;
}

std::string State::GetGameClockString() const {
    return GameClock::FormatClock(elapsedTicks);
}

int State::CalculateScore() const {
    // Amiga: elapsedTicks advances 1 tick every 6 VBlanks (150 ticks/hour,
    // confirmed this session; DOS's own tick rate is different, 546/hour,
    // and not assumed to carry over). Epoch is midnight June 16 (hour 0),
    // so hours elapsed is a direct, un-offset proxy for time survived.
    return GameClock::TicksToHours(elapsedTicks);
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
    // Scored the same way an opponent's car spec is (see racer_pool.cpp): the
    // car's top speed in mph. SrCarSpecEntry is not involved - it is a raw
    // 14-byte SR_data record and has no room for player state - so this goes
    // through the Car's own parts instead.
    if (!currentCar) {
        return srrace::ComputeTopSpeed(srrace::CarSetup{});
    }
    return currentCar->GetTopSpeed();
}

const SrDriverData* State::GetCurrentDriverData() {
    // The opponent carries the SR_data row it was built from, which is valid
    // whoever selected them. The pool index only exists when the pool picked
    // this challenger, so it is the fallback rather than the primary lookup.
    const Opponent* opp = GetCurrentOpponent();
    if (opp != nullptr && opp->driver.srDriverIndex >= 0 &&
        opp->driver.srDriverIndex < static_cast<int>(srDriverData.size())) {
        return &srDriverData[opp->driver.srDriverIndex];
    }

    if (lastChallengerPoolIndex < 0 || lastChallengerPoolIndex >= RACER_POOL_SIZE ||
        lastChallengerPoolIndex >= static_cast<int>(racerPool.pool.size())) {
        return nullptr;
    }
    const int driverIndex = racerPool.pool[lastChallengerPoolIndex].driver_index;
    if (driverIndex < 0 || driverIndex >= static_cast<int>(srDriverData.size())) {
        return nullptr;
    }
    return &srDriverData[driverIndex];
}

BetContext State::GetBetContext() {
    BetContext ctx;
    if (currentCar) {
        ctx.playerCarClass    = currentCar->GetWeightClass();
        ctx.playerEngineClass = currentCar->GetEngineClass();
        const SrLibCarDecal decal = currentCar->GetDecal();
        // The sticker nibble is one-based; a custom decal is not in the
        // original's set, so it counts as the first one.
        ctx.stickerIndex = decal == SrLibCarDecalNone   ? 0
                         : decal == SrLibCarDecalCustom ? 1
                                                        : static_cast<int>(decal) + 1;
    }

    const SrCarSpecEntry* spec = GetChallengerSpec();
    if (spec != nullptr) {
        ctx.oppCarClass = spec->car_class;
        ctx.oppEngineClass = spec->EngineClass();
    } else {
        // No pool-backed challenger: read as evenly matched.
        ctx.oppCarClass    = ctx.playerCarClass;
        ctx.oppEngineClass = ctx.playerEngineClass;
    }

    const SrDriverData* driver = GetCurrentDriverData();
    if (driver != nullptr) ctx.driverTier = driver->tier;
    ctx.rivalryMargin = racerPool.RivalryMargin(lastChallengerPoolIndex);
    return ctx;
}

const SrCarSpecEntry* State::GetChallengerSpec() const {
    // The King sits at RACER_KING_SLOT, one past the pool, so his car is not
    // in pool[] - it is the last SR_data spec.
    if (lastChallengerPoolIndex == RACER_KING_SLOT &&
        SR_DATA_KING_CARSPEC_INDEX < static_cast<int>(mergedCarSpecs.size())) {
        return &mergedCarSpecs[SR_DATA_KING_CARSPEC_INDEX];
    }
    if (lastChallengerPoolIndex < 0 || lastChallengerPoolIndex >= RACER_POOL_SIZE ||
        lastChallengerPoolIndex >= static_cast<int>(racerPool.pool.size())) {
        return nullptr;
    }
    const int specIndex = racerPool.pool[lastChallengerPoolIndex].car_spec_index;
    if (specIndex < 0 || specIndex >= static_cast<int>(mergedCarSpecs.size())) {
        return nullptr;
    }
    return &mergedCarSpecs[specIndex];
}

int State::GetChallengerScore() const {
    // The King sits at RACER_KING_SLOT, one past the pool, so his car is not
    // in pool[] - it is the last SR_data spec.
    if (lastChallengerPoolIndex == RACER_KING_SLOT &&
        SR_DATA_KING_CARSPEC_INDEX < static_cast<int>(mergedCarSpecs.size())) {
        return ComputeCarPerformanceScore(mergedCarSpecs[SR_DATA_KING_CARSPEC_INDEX]);
    }
    // No pool-backed challenger: read as evenly matched. Anything below 91 is
    // a score no car in the game can have (ComputeTopSpeed's floor), and would
    // make the player look stronger than every opponent alive.
    if (lastChallengerPoolIndex < 0 ||
        lastChallengerPoolIndex >= RACER_POOL_SIZE) return GetPlayerScore();
    return racerPool.GetEntryScore(lastChallengerPoolIndex);
}

CarPtr State::BuildCarForSpec(int specIndex) {
    if (specIndex < 0 || specIndex >= (int)mergedCarSpecs.size() ||
        specIndex >= (int)mergedCarSpecModelIndex.size()) {
        return nullptr;
    }
    const int modelIdx = mergedCarSpecModelIndex[specIndex];
    if (modelIdx < 0) return nullptr;  // The King's slot, no buyable car
    CarModel model = carData.GetModelFromIndex(modelIdx);
    CarPtr car = CarBuilder::BuildCar(model);
    if (!car) return nullptr;
    CarBuilder::ApplySpec(car.get(), mergedCarSpecs[specIndex], model.vendor);
    return car;
}

Opponent* State::GetKingOpponent() {
    if (SR_DATA_KING_CARSPEC_INDEX >= static_cast<int>(mergedCarSpecs.size())) {
        return nullptr;
    }
    return roster.GetKingOpponent(carData, mergedCarSpecs[SR_DATA_KING_CARSPEC_INDEX]);
}

void State::PickNextOpponent() {
    // Fall back to Roster shuffle if pool is not initialised.
    if (racerPool.pool.empty()) {
        return;
    }

    // Somebody already queued a specific challenger (the diner's phone list),
    // so leave their pick alone.
    if (roster.HasForcedOpponent()) {
        return;
    }

    int playerScore = GetPlayerScore();

    // Check for King unlock. Winning his drag qualifier forces the road race
    // right back at him, bypassing the normal tier check entirely (king.md:
    // "before the player gets to choose anything, SetupChallengerEncounter(21)").
    bool kingUnlocked = kingQualifierWon || reputation.IsKingUnlocked(racerPool.CountActive());
    // ShowOpponentRefusal (0x0023e904): after an opponent turns down a bet,
    // the reroll has a flat 1/100 chance of being The King regardless of
    // reputation - confirmed via SelectNextChallenger's GetRandom16or32(100)
    // roll. He is still gated by the tier/fuel checks in Challenge::OpenKing
    // if the player actually tries to challenge him this early.
    bool kingCameo = false;
    if (!kingUnlocked && pendingKingCameoRoll) {
        kingCameo = std::uniform_int_distribution<int>(0, 99)(RandomEngine()) == 0;
    }
    pendingKingCameoRoll = false;
    if (kingUnlocked || kingCameo) {
        LOG_INFO("State::PickNextOpponent - The King (unlocked=%d cameo=%d)", kingUnlocked, kingCameo);
        lastChallengerPoolIndex = RACER_KING_SLOT;
        if (Opponent* king = GetKingOpponent()) {
            roster.SetNextOpponent(king);
        }
        return;
    }

    int poolIdx = racerPool.SelectNextChallenger(playerScore, lastChallengerPoolIndex);
    if (poolIdx < 0) {
        LOG_INFO("State::PickNextOpponent - pool exhausted, falling back to roster");
        return;
    }
    lastChallengerPoolIndex = poolIdx;

    // A rival who has been turning up for a while has been spending money on
    // their own car - see RacerPool::ApplyRivalryUpgrades. Runs before the
    // look is handed to the Roster below.
    racerPool.ApplyRivalryUpgrades(poolIdx);

    // Map pool entry → Roster Opponent so existing Diner code works unchanged.
    RacerEntry& entry = racerPool.pool[poolIdx];
    int driverIdx = std::min(entry.driver_index, (int)roster.GetOpponentCount() - 1);
    Opponent* opp = roster.GetOpponentByIndex(driverIdx);
    if (opp) {
        // Carless: the player took this driver's car in a pink-slip race
        // (OnRaceFinished resets it). RacerPool::OnRaceWon already dropped
        // them to a lower-classification spec, so build that car - a driver
        // with no car at all crashes everything downstream that reads it.
        if (!opp->car) {
            opp->car = BuildCarForSpec(entry.car_spec_index);
            if (!opp->car) {
                LOG_INFO("State::PickNextOpponent - pool[%d] driver=%d has no car, skipping",
                         poolIdx, entry.driver_index);
                return;
            }
        }
        // Give the opponent the look the pool rolled for this slot, so the
        // same rival always turns up in the same paint and body trim. The
        // car itself still comes from the Roster - wiring the pool's
        // car_spec_index through to the raced car is a separate gap.
        if (opp->car) {
            opp->car->SetColor(static_cast<StandardCarColor>(entry.color_index));
            opp->car->SetDecal(CarBodyDecal(entry.body_byte));
            if (CarModelCanChopRoof(opp->car->GetModelRef())) {
                opp->car->SetRoofChopped(CarBodyRoofChopped(entry.body_byte));
            }
            if (CarModelCanStripBumpers(opp->car->GetModelRef())) {
                opp->car->SetBumpersStripped(CarBodyBumpersStripped(entry.body_byte));
            }
        }
        roster.SetNextOpponent(opp);
        LOG_INFO("State::PickNextOpponent - pool[%d] driver=%d car_spec=%d score=%d",
                 poolIdx, entry.driver_index, entry.car_spec_index, playerScore);
    }
}

void State::RecordChallengerRace(bool opponentWon) {
    if (lastChallengerPoolIndex < 0 || lastChallengerPoolIndex >= RACER_POOL_SIZE) return;
    racerPool.RecordRace(lastChallengerPoolIndex, opponentWon);
}

bool State::OnRaceFinished(RaceType raceType, bool won) {
    const bool pinkSlip = pendingIsPinkSlip;
    if (won) {
        reputation.RecordWin(raceType);
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceWon(lastChallengerPoolIndex, pinkSlip);
        }
        if (pinkSlip) {
            // The stake was the cars: take theirs. They are left without one -
            // they have just lost it, after all.
            Opponent* opponent = GetCurrentOpponent();
            if (opponent != nullptr && opponent->car) {
                CarPtr wonCar = opponent->car;
                opponent->car.reset();
                ownedCars.push_back(wonCar);
                // Drive it away: winning the pink slip means it is yours now,
                // and the old car stays in the garage alongside it.
                currentCar = wonCar;
                LOG_INFO("State::OnRaceFinished - pink slip win: now driving %s",
                         wonCar->GetModel().name.c_str());
            }
        } else {
            bankroll += pendingBetAmount;
        }
        LOG_INFO("State::OnRaceFinished - WIN $%d bankroll=%d drag_rep=%d road_rep=%d pool_active=%d",
                 pendingBetAmount, bankroll, reputation.drag_rep, reputation.road_rep,
                 racerPool.CountActive());

        if (lastChallengerPoolIndex == RACER_KING_SLOT) {
            if (raceType == RaceType::Drag) {
                // Qualifier only; his road race (forced next) is the decider.
                kingQualifierWon = true;
            } else {
                LOG_INFO("State::OnRaceFinished - King beaten in the ROAD race! Triggering end-game.");
                return true;
            }
        }
    } else {
        reputation.RecordLoss();
        if (!racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
            lastChallengerPoolIndex < RACER_POOL_SIZE) {
            racerPool.OnRaceLost(lastChallengerPoolIndex);
        }
        // Losing either King race clears the qualifier: the drag must be won
        // again before the road race is offered.
        if (lastChallengerPoolIndex == RACER_KING_SLOT) {
            kingQualifierWon = false;
        }
        if (pinkSlip) {
            // The stake was the cars, so the one that was raced is gone.
            ScrapCurrentCar();
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

// Where the hall of fame is kept. Neither Amiga disk ships a hall_dat - the
// original creates it on first save - so the pref dir is the only copy, and
// the game directory is only read as a fallback (a DOS install has HALL_DAT
// next to SR.EXE, and older RodLegacy builds wrote it there).
static std::vector<fs::path> HallDatPaths(const fs::path& gameBase) {
    std::vector<fs::path> paths;
    fs::path pref = AssetManager::GetPrefDir();
    if (!pref.empty()) paths.push_back(pref / "hall_dat");
    paths.push_back(gameBase / "hall_dat");
    paths.push_back(gameBase / "HALL_DAT");
    return paths;
}

std::vector<State::HallEntry> State::LoadHallDat() const {
    for (const fs::path& path : HallDatPaths(assets.GetBasePath())) {
        std::ifstream f(path, std::ios::binary);
        if (!f) continue;
        std::vector<uint8_t> buf(HALL_DAT_FILE_SIZE, 0);
        f.read(reinterpret_cast<char*>(buf.data()), HALL_DAT_FILE_SIZE);
        auto entries = ParseHallDat(buf.data(), (size_t)f.gcount());
        LOG_INFO("LoadHallDat: loaded %zu entries from %s", entries.size(), path.string().c_str());
        return entries;
    }

    LOG_INFO("LoadHallDat: hall_dat not found, starting with empty table");
    return {};
}

void State::SaveHallDat(const std::vector<HallEntry>& entries) const {
    uint8_t buf[HALL_DAT_FILE_SIZE] = {};

    // Header: int16 entry count (big-endian)
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

    fs::path target = HallDatPaths(assets.GetBasePath()).front();
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    std::ofstream f(target, std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(reinterpret_cast<const char*>(buf), HALL_DAT_FILE_SIZE);
        LOG_INFO("SaveHallDat: saved %d entries to %s", count, target.string().c_str());
    } else {
        LOG_INFO("SaveHallDat: failed to write %s", target.string().c_str());
    }
}

// Global object
State state;
