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
#include "scenes/sr2_ending.h"
#include "scenes/grudge_night.h"
#include "scenes/intro.h"
#ifdef RODLEGACY_DEBUG_UI
#include "debug/car_debug.h"
#include "debug/sfx_debug.h"
#endif
#include "race/racing.h"
#include "debug/debug_overlay.h"
#include "parsers/libp.h"
#include "car/car_builder.h"
#include "car/car_set.h"
#include "core/save_loader.h"
#include "race/bet_acceptance.h"
#include "race/track_gen.h"
#include "race/aqueduct.h"
#include "core/utils.h"
#include <fstream>
#include <cstring>
#include <random>

State::State() : bankroll(5750) {
}

void State::Init(bool debugMode, const fs::path& gamePath, bool muteAudio, SrGame game) {
    this->debugMode = debugMode;
    try {
        assets.Init(gamePath, muteAudio, game);
        assets.ReadAssets();
        DataParser dataParser;

        // The car spec record and the text blob's position are what differ
        // between the games; the driver record is identical.
        const bool isSr2 = assets.GetGame() == SrGame::SR2;
        srCarSpecData = isSr2 ? dataParser.ParseCarSpecTableSR2(assets.GetSrData())
                              : dataParser.ParseCarSpecTable(assets.GetSrData());
        LoadBetTables(assets.GetSrData());

        // The physics tables come out of the player's own executable, the way the bet tables come out of
        // SR_data.
        if (!srrace::LoadPhysicsTables(assets.GetSrExeData(),
                                       isSr2 ? srrace::PhysicsTableSource::StreetRod2
                                             : srrace::PhysicsTableSource::StreetRod)) {
            LOG_WARN("Physics tables not found in the executable, using built-in fallbacks");
        }
        if (!srrace::LoadScoringTables(assets.GetSrExeData(),
                                       isSr2 ? srrace::PhysicsTableSource::StreetRod2
                                             : srrace::PhysicsTableSource::StreetRod)) {
            LOG_WARN("Scoring tables not found in the executable, using built-in fallbacks");
        }
        if (!srrace::LoadTrackTables(assets.GetSrExeData(),
                                       isSr2 ? srrace::TrackGenSource::StreetRod2
                                             : srrace::TrackGenSource::StreetRod)) {
            LOG_WARN("Track variant table not found in the executable, using built-in fallbacks");
        }
        // The Aqueduct banks by rotating its projected rows, and the angles
        // come out of the same executable.
        if (isSr2 && !srrace::AqueductLoadSineTable(assets.GetSrExeData())) {
            LOG_WARN("Sine table not found in the executable, using computed values");
        }
        // Its obstacles are lib.p assets sliced by a table in the same place.
        if (isSr2 && !srrace::AqueductLoadShapeTable(assets.GetSrExeData())) {
            LOG_WARN("Aqueduct shape table not found in the executable, using built-in copy");
        }
        if (isSr2 && !srrace::AqueductLoadStarTable(assets.GetSrExeData())) {
            LOG_WARN("Aqueduct star table not found in the executable, using built-in copy");
        }

        // Reads every enabled set's cars and drivers, the played game's first,
        // and hands the roster the merged driver table.
        RebuildCarSets();

        roster.Initialize(carData);
        GiveOpponentsPoolCars();

        // FIXME: until cars can be bought, start with one of every car on sale.
        for (int assetId : mergedCarSpecModelIndex) {
            if (assetId < 0) continue;
            ownedCars.push_back(roster.GenerateRandomCar(carData.GetModelFromIndex(assetId)));
        }

        GenerateClassifieds();

        // FIXME: Until we can buy a car
        this->currentCar = ownedCars.at(0);
        LOG_INFO("Current car tires: %d", this->currentCar->GetTires().assetId);

        garage = std::make_unique<Garage>();
        diner = std::make_unique<Diner>();
        gasStation = std::make_unique<GasStation>();
        driving = std::make_unique<Driving>();
        newspaper = std::make_unique<Newspaper>();
        nameEntry = std::make_unique<NameEntry>();
        saveLoad = std::make_unique<SaveLoadScene>();
        gameOverMenu = std::make_unique<GameOverMenu>();
        ending = std::make_unique<Ending>();
        sr2Ending = std::make_unique<Sr2Ending>();
        intro = std::make_unique<Intro>();
        grudgeNight = std::make_unique<GrudgeNight>();
#ifdef RODLEGACY_DEBUG_UI
        carDebug = std::make_unique<CarDebug>();
        sfxDebug = std::make_unique<SfxDebug>();
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
        sr2Ending->Init(debugMode);
        intro->Init(debugMode);
        grudgeNight->Init(debugMode);
#ifdef RODLEGACY_DEBUG_UI
        carDebug->Init(debugMode);
        sfxDebug->Init(debugMode);
#endif
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        throw;
    }
}

State::~State() = default;

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
    // The second half of a drive: fuel and cutscene were dealt with on the way in.
    const bool fromCruise = arrivingFromCruise;
    arrivingFromCruise = false;

    // Only drives between places cost fuel.
    auto isDriveTo = [](SrLibGameScene s) {
        return s == SCENE_GARAGE || s == SCENE_DINER || s == SCENE_GAS_STATION ||
               s == SCENE_DRIVING || s == SCENE_CRUISING;
    };
    if (!fromCruise && scene != previousScene && isDriveTo(previousScene) && isDriveTo(scene)) {
        BurnFuel(FUEL_PER_TRIP_TENTHS);
    }

    // Driving starts the cutscene from its own OnSceneEnter; starting it here would be reset.
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
            this->currentScene = assets.GetGame() == SrGame::SR2 ? static_cast<Scene*>(sr2Ending.get())
                                                                 : static_cast<Scene*>(ending.get());
            break;
        case SCENE_INTRO:
            this->currentScene = intro.get();
            break;
        case SCENE_GRUDGE_NIGHT:
            this->currentScene = grudgeNight.get();
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
        case SCENE_SFX_DEBUG:
#ifdef RODLEGACY_DEBUG_UI
            this->currentScene = sfxDebug.get();
#else
            LOG_INFO("SCENE_SFX_DEBUG is only available in debug_ui builds");
            currentGameScene = previousScene;
            return;
#endif
            break;
    }

    // Before FadeIn: the fade would fade up the previous visit's leftovers.
    this->currentScene->OnSceneEnter();
    this->currentScene->FadeIn();
}

// Called again on a new game, since the chosen sets change the table saved model indices resolve against.
void State::RebuildCarSets() {
    DataParser dataParser;
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    const int bootSet = CarSetForGame(assets.GetGame());

    // The played game goes in first, so its specs keep indices 0..25 - the
    // range original .SAV files store - and its King keeps driver index 21.
    std::vector<int> order{bootSet};
    for (int set = 0; set < (int)defs.size(); set++) {
        if (set != bootSet && assets.IsCarSetEnabled(set)) order.push_back(set);
    }

    mergedCarSpecs.clear();
    mergedCarSpecModelIndex.clear();
    srDriverData.clear();
    srDriverNames.clear();
    srDriverSets.clear();

    for (int set : order) {
        const CarSetDef& def = defs[set];
        // A set with no files of its own reads one of the games'.
        const bool ownFiles = def.zipName.empty();
        const bool isSr2 = ownFiles && def.game == SrGame::SR2;

        std::vector<SrCarSpecEntry> specs;
        std::vector<CarSpriteRefs> sprites;
        int added = 0;

        if (ownFiles) {
            const byte_vector& data = assets.GetSrData(def.game);
            added = carData.ReadCarSetAmiga(set, data, assets.GetImageEntries(def.game));
            specs = isSr2 ? dataParser.ParseCarSpecTableSR2(data)
                          : dataParser.ParseCarSpecTable(data);
            sprites = dataParser.ParseCarSpritesAmiga(data, def.game);
        } else {
            const byte_vector& data = assets.GetCarSetData(set);
            added = carData.ReadCarSet(set, data, assets.GetCarSetImageEntries(set));
            specs = dataParser.ParseCarSpecTableDOS(data);
            sprites = dataParser.ParseCarSpritesDOS(data);
        }

        if (def.providesCars && added > 0) {
            for (int i = 0; i < (int)specs.size(); i++) {
                mergedCarSpecs.push_back(specs[i]);
                // A car that is not for sale has no model behind it: that is
                // The King's slot, one per set.
                const bool buyable = specs[i].ForSale() && i < (int)sprites.size() &&
                                     sprites[i].body >= 0;
                const int body = buyable ? sprites[i].body : -1;
                mergedCarSpecModelIndex.push_back(
                    !buyable ? -1
                             : (set == kAmigaCarSet && ownFiles
                                    ? body
                                    : set * HOT_DATA_SPRITE_BASE + body));
            }
        }

        if (!def.providesDrivers || !ownFiles) continue;

        const byte_vector& data = assets.GetSrData(def.game);
        const std::vector<SrDriverData> drivers = dataParser.ParseDriverData(data);
        const std::vector<std::string> names =
            isSr2 ? dataParser.ParseDriverNamesSR2(data) : dataParser.ParseDriverNames(data);
        for (int i = 0; i < (int)drivers.size() && i < (int)names.size(); i++) {
            // Only the played game's King races; another game's is just a
            // driver record with nothing behind it.
            if (set != bootSet && i == SR_DATA_KING_DRIVER_INDEX) continue;
            srDriverData.push_back(drivers[i]);
            srDriverNames.push_back(names[i]);
            srDriverSets.push_back(set);
        }
    }

    LOG_INFO("Merged tables: %zu car specs, %zu drivers across %zu sets",
             mergedCarSpecs.size(), srDriverData.size(), order.size());

    // Give the roster the real driver table before it builds opponents, so
    // names and portraits come from the data rather than a random sprite.
    roster.SetDriverTable(srDriverData, srDriverNames, srDriverSets);

    if (!srDriverData.empty() && !mergedCarSpecs.empty()) {
        // Which cars can wear which modifications.
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
        LOG_INFO("Car/driver tables empty (drivers=%zu carspecs=%zu) - pool disabled",
                 srDriverData.size(), mergedCarSpecs.size());
    }
}

void State::GenerateClassifieds() {
    classifiedCars.clear();
    ++classifiedsGeneration;

    // Merged-table order: the newspaper walks the same table and its buy handler indexes by list position.
    for (int spec = 0; spec < static_cast<int>(mergedCarSpecs.size()); spec++) {
        const int carAssetId = mergedCarSpecModelIndex[spec];
        if (carAssetId < 0) continue;  // The King's slot, not for sale
        CarMetadata_t metadata = carData.GetCarMetadata(carAssetId);
        if (!metadata.classified.carName || strlen(metadata.classified.carName) == 0) {
            continue;
        }
        CarModel model = carData.GetModelFromIndex(carAssetId);
        CarPtr car = roster.GenerateRandomCar(model);
        car->SetRoofChopped(false);
        car->SetBumpersStripped(false);
        car->SetDecal(SrLibCarDecalNone);
        CarBuilder::ApplySpec(car.get(), mergedCarSpecs[spec], model.vendor);
        classifiedCars.push_back(car);
    }
    LOG_INFO("Generated %zu classified cars", classifiedCars.size());
}

void State::StartNewGame() {
    // The chosen car sets decide the spec table and the opponents' cars.
    RebuildCarSets();
    roster.Initialize(carData);
    GiveOpponentsPoolCars();
    GenerateClassifieds();
    ownedCars.clear();
    currentCar.reset();
    ClearSpares();
    jailed = false;
    kingQualifierWon = false;
    kingListed = false;
    // One roll per profile, fixed for its life.
    SetTrackVariant(std::uniform_int_distribution<int>(0, 9)(RandomEngine()));
    scatterSeed = std::uniform_int_distribution<uint32_t>(1, 0xffffffffu)(RandomEngine());
}

void State::ClearSpares() {
    spareTires.clear();
    spareTransmissions.clear();
    spareManifolds.clear();
    spareCarburetors.clear();
    spareEngines.clear();
    spareExhausts.clear();
    spareMufflers.clear();
    spareDifferentials.clear();
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

// The cheapest car in SR_data; the original hardcodes the same.
bool State::IsOutOfDough() const {
    return currentCar == nullptr && ownedCars.empty() &&
           spareTires.empty() && spareTransmissions.empty() && spareManifolds.empty() &&
           spareCarburetors.empty() && spareEngines.empty() &&
           bankroll < 400;
}

void State::ScrapCurrentCar() {
    // Freed once nothing shows it any more.
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

// rec.specIndex is the save's 0-25 spec index, not the lib.p asset id CarData::GetModelFromIndex takes;
// kCarIndex maps between them for the 25 ownable cars.
CarPtr BuildCarFromRecord(const LoadedCarRecord& rec, CarData& carData,
                        const std::vector<SrCarSpecEntry>& specs) {
    if (rec.specIndex < 0 || rec.specIndex >= CAR_INDEX_COUNT) {
        LOG_INFO("LoadSaveGame: car spec index %d has no known lib.p asset (King's car or out of range), using a placeholder model", rec.specIndex);
    }
    int assetId = (rec.specIndex >= 0 && rec.specIndex < CAR_INDEX_COUNT) ? kCarIndex[rec.specIndex] : kCarIndex[0];
    CarModel model = carData.GetModelFromIndex(assetId);
    CarPtr car = CarBuilder::BuildCar(model);

    // The class byte belongs to the model, so it comes from the spec.
    if (rec.specIndex >= 0 && rec.specIndex < static_cast<int>(specs.size())) {
        car->SetWeightClass(specs[rec.specIndex].car_class);
    }

    car->SetColor(static_cast<StandardCarColor>(rec.colorIndex));
    // The sticker nibble is one-based.
    car->SetRoofChopped(CarBodyRoofChopped(rec.bodyByte));
    car->SetBumpersStripped(CarBodyBumpersStripped(rec.bodyByte));
    car->SetDecal(CarBodyDecal(rec.bodyByte));
    // Signed -8..+4 in the save maps onto the unsigned knob with nothing clamped.
    car->SetTuneLevel(kPerfectTuneLevel + rec.tuningKnob);
    car->SetFuelTenths(rec.fuelTenths);

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
                    car->SetEngine(enginePart);
                }
                break;
            }
            case 2: { // Manifold - decode owned by CarBuilder::ManifoldTypeFromSaveVariant
                ManifoldType type;
                if (!CarBuilder::ManifoldTypeFromSaveVariant(p.variant, type)) type = MANIFOLD_2BARREL;
                Part part = CarBuilder::CreateManifoldPart(type, model.vendor);
                // Manifolds don't track wear; wearRaw is the 0xFF80 sentinel.
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
                // Carbs don't track wear; wearRaw is the 0xFF80 sentinel.
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
    // Last, against the fitted parts: the save carries the car's live value.
    car->SetValue(rec.value);
    return car;
}

}

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

    // Anything still showing an old car keeps it alive.
    ownedCars.clear();
    currentCar.reset();
    ClearSpares();
    SetKnownOpponentNames({});

    if (loaded.hasActiveCar) {
        CarPtr active = BuildCarFromRecord(loaded.activeCar, carData, srCarSpecData);
        ownedCars.push_back(active);
        currentCar = active;
    }
    for (const auto& rec : loaded.roster) {
        ownedCars.push_back(BuildCarFromRecord(rec, carData, srCarSpecData));
    }
    if (!currentCar && !ownedCars.empty()) currentCar = ownedCars.at(0);

    // Slot 21 is The King, who has no pool entry.
    for (int i = 0; i < RACER_POOL_SIZE; ++i) {
        racerPool.SetCarSpecForSlot(i, loaded.opponentCarSpec[i]);
        if (i < static_cast<int>(racerPool.pool.size())) racerPool.pool[i].status = 0;
    }
    for (int i = 0; i < RACER_POOL_SIZE && i < static_cast<int>(racerPool.pool.size()); ++i) {
        if (loaded.opponentMet[i]) AddKnownOpponent(GiveOpponentPoolCar(racerPool.pool[i]));
    }

    // The save stores standing, not wins, so drag_wins/road_wins stay 0.
    reputation = Reputation();
    reputation.races_completed = loaded.racesCompleted;
    reputation.drag_rep        = loaded.dragRep;
    reputation.road_rep        = loaded.roadRep;
    reputation.last_race_won   = loaded.lastRaceWon;
    kingQualifierWon           = loaded.lastRaceWon;

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
    // 150 ticks/hour from midnight June 16, so hours elapsed is time survived.
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

void State::SetKnownOpponentNames(const std::vector<std::string>& names) {
    roster.ClearKnownOpponents();
    for (const std::string& name : names) {
        Opponent* opp = roster.FindOpponentByName(name);
        if (!opp) {
            Opponent* king = GetKingOpponent();
            if (king && king->driver.name == name) opp = king;
        }
        if (opp) roster.AddKnownOpponent(opp);
    }
}

const std::vector<Opponent*>& State::GetKnownOpponents() const {
    return roster.GetKnownOpponents();
}

void State::SetNextOpponent(Opponent* opp) {
    roster.SetNextOpponent(opp);
    if (!opp) return;
    if (roster.IsKing(opp)) {
        lastChallengerPoolIndex = RACER_KING_SLOT;
        return;
    }
    for (size_t i = 0; i < racerPool.pool.size() && i < RACER_POOL_SIZE; i++) {
        if (racerPool.pool[i].driver_index == opp->driver.srDriverIndex) {
            lastChallengerPoolIndex = static_cast<int>(i);
            return;
        }
    }
}

int State::GetPlayerScore() const {
    // Top speed from the Car's own parts, the same score an opponent's spec gets.
    if (!currentCar) {
        return srrace::ComputeTopSpeed(srrace::CarSetup{});
    }
    return currentCar->GetTopSpeed();
}

const SrDriverData* State::GetCurrentDriverData() {
    // The opponent's own SR_data row, valid whoever selected them; the pool index is the fallback.
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
        // A custom decal counts as the first sticker.
        ctx.stickerIndex = decal == SrLibCarDecalNone   ? 0
                         : decal == SrLibCarDecalCustom ? 1
                                                        : static_cast<int>(decal) + 1;
    }

    const SrCarSpecEntry* spec = GetChallengerSpec();
    if (spec != nullptr) {
        ctx.oppCarClass = spec->car_class;
        ctx.oppEngineClass = spec->EngineClass();
    } else {
        // No pool-backed challenger: evenly matched.
        ctx.oppCarClass    = ctx.playerCarClass;
        ctx.oppEngineClass = ctx.playerEngineClass;
    }

    const SrDriverData* driver = GetCurrentDriverData();
    if (driver != nullptr) ctx.driverTier = driver->tier;
    ctx.rivalryMargin = racerPool.RivalryMargin(lastChallengerPoolIndex);
    return ctx;
}

const SrCarSpecEntry* State::GetChallengerSpec() const {
    // The King's car is the last SR_data spec, not in pool[].
    if (lastChallengerPoolIndex == RACER_KING_SLOT &&
        SrKingCarSpecIndex(assets.GetGame()) < static_cast<int>(mergedCarSpecs.size())) {
        return &mergedCarSpecs[SrKingCarSpecIndex(assets.GetGame())];
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
    // The King's car is the last SR_data spec, not in pool[].
    if (lastChallengerPoolIndex == RACER_KING_SLOT &&
        SrKingCarSpecIndex(assets.GetGame()) < static_cast<int>(mergedCarSpecs.size())) {
        return ComputeCarPerformanceScore(mergedCarSpecs[SrKingCarSpecIndex(assets.GetGame())]);
    }
    // Evenly matched. Below 91 is a score no car can have and would make the player look stronger than everyone.
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

// The driver races the car the pool matched them on.
Opponent* State::GiveOpponentPoolCar(const RacerEntry& entry) {
    const int driverIdx = std::min(entry.driver_index, (int)roster.GetOpponentCount() - 1);
    Opponent* opp = roster.GetOpponentByIndex(driverIdx);
    const int spec = entry.car_spec_index;
    if (!opp || spec < 0 || spec >= (int)mergedCarSpecModelIndex.size()) return opp;
    const int modelIdx = mergedCarSpecModelIndex[spec];
    if (opp->car && opp->car->GetModelRef().index == modelIdx) return opp;
    opp->car = BuildCarForSpec(spec);
    opp->driver.preferredCar = modelIdx;
    return opp;
}

void State::GiveOpponentsPoolCars() {
    for (const RacerEntry& entry : racerPool.pool) GiveOpponentPoolCar(entry);
}

Opponent* State::GetKingOpponent() {
    if (SrKingCarSpecIndex(assets.GetGame()) >= static_cast<int>(mergedCarSpecs.size())) {
        return nullptr;
    }
    return roster.GetKingOpponent(carData, mergedCarSpecs[SrKingCarSpecIndex(assets.GetGame())],
                                  assets.GetGame());
}

void State::PickNextOpponent() {
    if (racerPool.pool.empty()) {
        return;
    }

    // A specific challenger was already queued (the diner's phone list).
    if (roster.HasForcedOpponent()) {
        return;
    }

    int playerScore = GetPlayerScore();

    // After his drag qualifier the King stays for the road race; otherwise every arrival rolls for him.
    const int kingChance = KingArrivalChance();
    // Street Rod 2's King does not wait between his three races; each one is a fresh roll (FUN_0024ff92).
    const bool king = kingQualifierWon ||
                      std::uniform_int_distribution<int>(0, 99)(RandomEngine()) < kingChance;
    if (king) {
        LOG_INFO("State::PickNextOpponent - The King (qualifier=%d chance=%d%%)", kingQualifierWon, kingChance);
        lastChallengerPoolIndex = RACER_KING_SLOT;
        if (Opponent* kingOpp = GetKingOpponent()) {
            roster.SetNextOpponent(kingOpp);
        }
        return;
    }

    const int playerClass = currentCar ? currentCar->GetWeightClass() : 0;
    int poolIdx = racerPool.SelectNextChallenger(playerScore, playerClass, lastChallengerPoolIndex);
    if (poolIdx < 0) {
        LOG_INFO("State::PickNextOpponent - pool exhausted, falling back to roster");
        return;
    }
    lastChallengerPoolIndex = poolIdx;

    racerPool.ApplyRivalryUpgrades(poolIdx);

    RacerEntry& entry = racerPool.pool[poolIdx];
    Opponent* opp = GiveOpponentPoolCar(entry);
    if (opp && !opp->car) {
        LOG_INFO("State::PickNextOpponent - pool[%d] driver=%d has no car, skipping",
                 poolIdx, entry.driver_index);
        return;
    }
    if (opp) {
        // The same rival turns up in the same paint and trim.
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

int State::KingArrivalChance() const {
    const bool sr2 = assets.GetGame() == SrGame::SR2;
    // FUN_0024fcdc: never twice running.
    if (sr2 && lastChallengerPoolIndex == RACER_KING_SLOT) return 0;
    switch (reputation.GetOpponentTier(racerPool.CountActive(), sr2)) {
        case 0:  return kingListed ? 10 : 50;
        case 1:  return 5;
        case 2:  return 8;
        default: return 12;
    }
}

bool State::KingOnlyDrivesBy() const {
    return reputation.GetOpponentTier(racerPool.CountActive(), assets.GetGame() == SrGame::SR2) == 1;
}

void State::OnPhoneListOpened() {
    kingListed = reputation.IsKingUnlocked(racerPool.CountActive(), assets.GetGame() == SrGame::SR2);
}

bool State::OnRaceFinished(RaceType raceType, bool won, bool settlesBet) {
    const bool pinkSlip = pendingIsPinkSlip;
    const bool vsKing = (lastChallengerPoolIndex == RACER_KING_SLOT);
    const bool counted = pinkSlip || pendingBetAmount > 0 || vsKing;
    const bool inPool = !racerPool.pool.empty() && lastChallengerPoolIndex >= 0 &&
                        lastChallengerPoolIndex < RACER_POOL_SIZE;

    const bool sr2 = assets.GetGame() == SrGame::SR2;
    reputation.RecordRace(raceType, won, counted, vsKing, sr2);
    if (counted && inPool) racerPool.RecordRace(lastChallengerPoolIndex, !won);

    if (vsKing && !won) {
        kingQualifierWon = false;
        kingListed = false;
        reputation.king_stage = 0;
    }

    if (settlesBet) {
        if (won) {
            if (inPool) racerPool.OnRaceWon(lastChallengerPoolIndex, pinkSlip);
            if (pinkSlip) {
                // The stake was the cars: take theirs. The player only drives it if it is a better class.
                Opponent* opponent = GetCurrentOpponent();
                if (opponent != nullptr && opponent->car) {
                    CarPtr wonCar = opponent->car;
                    opponent->car.reset();
                    Part tires = wonCar->GetTires();
                    tires.wear = std::uniform_int_distribution<int>(0, 499)(RandomEngine());
                    wonCar->SetTires(tires);
                    ownedCars.push_back(wonCar);
                    if (!currentCar || wonCar->GetWeightClass() > currentCar->GetWeightClass()) {
                        currentCar = wonCar;
                    }
                    LOG_INFO("State::OnRaceFinished - pink slip win: %s, driving %s",
                             wonCar->GetModel().name.c_str(),
                             currentCar->GetModel().name.c_str());
                }
            } else {
                bankroll += pendingBetAmount;
            }
        } else {
            if (inPool) racerPool.OnRaceLost(lastChallengerPoolIndex);
            if (pinkSlip) {
                // The stake was the cars, so the one that was raced is gone.
                ScrapCurrentCar();
            } else {
                bankroll = std::max(0, bankroll - pendingBetAmount);
            }
        }
    }
    LOG_INFO("State::OnRaceFinished - %s $%d settled=%d bankroll=%d drag_rep=%d road_rep=%d races=%d",
             won ? "WIN" : "LOSS", pendingBetAmount, settlesBet, bankroll, reputation.drag_rep,
             reputation.road_rep, reputation.races_completed);

    // Street Rod 2: drag, then Mulholland, then the Aqueduct, which wins the game (ldrivein.c 0x250eaa).
    if (vsKing && won && sr2) {
        if (raceType == RaceType::Aqueduct) {
            LOG_INFO("State::OnRaceFinished - King beaten on the Aqueduct! Triggering end-game.");
            return true;
        }
        reputation.king_stage = raceType == RaceType::Drag ? 1 : 2;
        return false;
    }
    if (vsKing && won) {
        if (raceType == RaceType::Drag) {
            // Qualifier only; his road race is the decider.
            kingQualifierWon = true;
        } else {
            LOG_INFO("State::OnRaceFinished - King beaten in the ROAD race! Triggering end-game.");
            return true;
        }
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

// Neither Amiga disk ships a hall_dat, so the pref dir is the only copy; the game directory is a read
// fallback (a DOS install keeps HALL_DAT next to SR.EXE).
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

    buf[0] = (HALL_DAT_ENTRY_COUNT >> 8) & 0xFF;
    buf[1] = HALL_DAT_ENTRY_COUNT & 0xFF;

    // Write up to 10 entries, XOR'd with 0x80
    int count = std::min((int)entries.size(), HALL_DAT_ENTRY_COUNT);
    for (int i = 0; i < count; i++) {
        int offset = HALL_DAT_HEADER_SIZE + i * HALL_DAT_ENTRY_SIZE;
        for (int b = 0; b < HALL_DAT_NAME_LEN; b++) {
            char c = (b < (int)entries[i].name.size()) ? entries[i].name[b] : '\0';
            buf[offset + b] = (uint8_t)(c ^ HALL_DAT_XOR_KEY);
        }
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

State state;
