#include "core/save_file.h"

#include "core/log.h"
#include "core/state.h"
#include "core/assets.h"
#include "car/car.h"
#include "car/car_builder.h"
#include "car/engine_bay.h"
#include "car/car_set.h"
#include "core/game_clock.h"

#include "toml.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

using SaveValue = toml::basic_value<toml::discard_comments, std::map, std::vector>;
using SaveArray = SaveValue::array_type;

namespace {

constexpr int kSaveVersion = 1;

SaveValue ParseDocument(const std::string& text) {
    try {
        std::istringstream in(text);
        return toml::parse<toml::discard_comments, std::map, std::vector>(in, "save.toml");
    } catch (const std::exception& e) {
        throw SaveLoadError(std::string("not valid TOML: ") + e.what());
    }
}

void CheckVersion(const SaveValue& root) {
    const int version = toml::find_or(root, "version", 0);
    if (version <= 0 || version > kSaveVersion) {
        throw SaveLoadError("unsupported save version " + std::to_string(version));
    }
}

void CheckGame(const SaveValue& root, SrGame running) {
    const std::string saved = toml::find_or(root, "game", std::string());
    if (!SrGameSaveKeyMatches(saved, running)) {
        throw SaveLoadError("this is a " + (saved.empty() ? std::string("sr1") : saved) +
                            " save and " + SrGameSaveKey(running) +
                            " is running; start that game to load it");
    }
}


SaveValue PartToToml(const Part& p, int typeValue) {
    SaveValue v;
    v["type"]   = typeValue;
    v["wear"]   = p.wear;
    v["cost"]   = p.cost;
    v["vendor"] = static_cast<int>(p.vendor);
    if (!p.name.empty()) v["name"] = p.name;
    return v;
}

SaveValue EngineToToml(const EnginePart& e) {
    SaveValue v;
    v["type"]   = static_cast<int>(e.engineType);
    v["wear"]   = e.wear;
    v["cost"]   = e.cost;
    v["vendor"] = static_cast<int>(e.vendor);
    if (!e.name.empty()) v["name"] = e.name;
    return v;
}

struct RawPart {
    int type = 0;
    int wear = 0;
    int cost = 0;
    CarVendor_t vendor = CarVendorGeneric;
    std::string name;
};

RawPart ReadRawPart(const SaveValue& v) {
    RawPart r;
    r.type   = toml::find_or(v, "type", 0);
    r.wear   = toml::find_or(v, "wear", 0);
    r.cost   = toml::find_or(v, "cost", 0);
    r.vendor = static_cast<CarVendor_t>(toml::find_or(v, "vendor", 0));
    r.name   = toml::find_or(v, "name", std::string());
    return r;
}

// Rebuilds through CarBuilder so asset ids and names stay defined in one place.
Part ApplyRaw(Part p, const RawPart& r) {
    p.wear = r.wear;
    p.cost = r.cost;
    if (!r.name.empty()) p.name = r.name;
    return p;
}

Part TransmissionFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    return ApplyRaw(CarBuilder::CreateTransmissionPart(
                        static_cast<TransmissionType>(r.type)), r);
}

Part ManifoldFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    return ApplyRaw(CarBuilder::CreateManifoldPart(
                        static_cast<ManifoldType>(r.type), r.vendor), r);
}

Part CarburetorFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    return ApplyRaw(CarBuilder::CreateCarburetorPart(
                        static_cast<CarburetorType>(r.type)), r);
}

Part TireFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    const CarWheelType type = static_cast<CarWheelType>(r.type);
    Part p(r.wear, r.cost, r.vendor, TireAssetForType(type), type);
    p.name = r.name;
    return p;
}

Part Sr2PartFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    Part p;
    p.vendor = r.vendor;
    p.partType.sr2Class = r.type;
    return ApplyRaw(p, r);
}

EnginePart EngineFromToml(const SaveValue& v) {
    const RawPart r = ReadRawPart(v);
    const CarEngine_t type = static_cast<CarEngine_t>(r.type);
    EnginePart e(type, r.wear, r.cost, r.vendor, EngineBodyAsset(type));
    // A hand-edited save can disagree with itself; the type is authoritative.
    e.vendor = e.GetVendor();
    e.name = r.name;
    return e;
}

SaveValue CarToToml(Car& c) {
    SaveValue v;
    // The pool key, not the .sav format's spec index.
    v["model_index"]         = c.GetModelRef().index;
    v["color"]               = static_cast<int>(c.GetColor());
    v["decal"]               = static_cast<int>(c.GetDecal());
    v["tune_level"]          = c.GetTuneLevel();
    v["sr2_tune"]            = c.GetSr2Tune();
    v["fuel_tenths"]         = c.GetFuelTenths();
    v["weight_class"]        = c.GetWeightClass();
    v["spec_engine_class"]   = c.GetSpecEngineClass();
    v["spec_manifold_class"] = c.GetSpecManifoldClass();
    v["roof_chopped"]        = c.IsRoofChopped();
    v["bumpers_stripped"]    = c.IsBumpersStripped();
    if (!c.GetCustomDecalPath().empty()) v["custom_decal_path"] = c.GetCustomDecalPath();

    const Part gearbox  = c.GetGearbox();
    const Part manifold = c.GetManifold();
    const Part carb     = c.GetCarburator();
    const Part tires    = c.GetTires();
    v["gearbox"]    = PartToToml(gearbox,  static_cast<int>(gearbox.partType.transmissionType));
    v["manifold"]   = PartToToml(manifold, static_cast<int>(manifold.partType.manifoldType));
    v["carburetor"] = PartToToml(carb,     static_cast<int>(carb.partType.carburetorType));
    v["tires"]      = PartToToml(tires,    static_cast<int>(tires.partType.tireType));
    v["engine"]     = EngineToToml(c.GetEngine());

    // A car can be left half apart. "carburetor" stays the first fitted one for older readers.
    std::vector<int> fittedCarbs;
    for (int i = 0; i < kMaxCarburetors; i++) {
        if (c.HasCarburetor(i)) fittedCarbs.push_back(i);
    }
    v["carburetor_mounts"] = fittedCarbs;
    v["value"]             = c.GetValue();
    v["engine_fitted"]     = c.HasEngine();
    v["gearbox_fitted"]    = c.HasGearbox();
    v["manifold_fitted"]   = c.HasManifold();
    v["exhaust"]           = c.GetExhaust();
    v["muffler"]           = c.GetMuffler();
    v["muffler_removed"]   = c.IsMufflerRemoved();
    v["differential"]      = c.GetDifferential();
    return v;
}

CarPtr CarFromToml(const SaveValue& v, CarData& carData) {
    CarModel model = carData.GetModelFromIndex(toml::find_or(v, "model_index", 0));
    CarPtr car = CarBuilder::BuildCar(model);

    car->SetWeightClass(toml::find_or(v, "weight_class", 4));
    car->SetColor(static_cast<StandardCarColor>(toml::find_or(v, "color", 0)));
    car->SetDecal(static_cast<SrLibCarDecal>(toml::find_or(v, "decal", 0)));
    car->SetCustomDecalPath(toml::find_or(v, "custom_decal_path", std::string()));
    car->SetRoofChopped(toml::find_or(v, "roof_chopped", false));
    car->SetBumpersStripped(toml::find_or(v, "bumpers_stripped", false));
    car->SetFuelTenths(toml::find_or(v, "fuel_tenths", 160));
    car->SetTuneLevel(toml::find_or(v, "tune_level", kPerfectTuneLevel));
    car->SetSr2Tune(toml::find_or(v, "sr2_tune", 0));

    if (v.contains("gearbox"))    car->SetGearbox(TransmissionFromToml(toml::find(v, "gearbox")));
    if (v.contains("manifold"))   car->SetManifold(ManifoldFromToml(toml::find(v, "manifold")));
    // SetCarburator fills every mount; carburetor_mounts then narrows it.
    if (v.contains("carburetor")) car->SetCarburator(CarburetorFromToml(toml::find(v, "carburetor")));
    // The whole Part: tires carry the classified-ad name.
    if (v.contains("tires")) car->SetTires(TireFromToml(toml::find(v, "tires")));
    if (v.contains("engine"))     car->SetEngine(EngineFromToml(toml::find(v, "engine")));

    // Last: SetEngine/SetManifold reset the spec classes.
    const int engineClass   = toml::find_or(v, "spec_engine_class", -1);
    const int manifoldClass = toml::find_or(v, "spec_manifold_class", -1);
    if (engineClass >= 0 || manifoldClass >= 0) {
        car->SetSpecPartClasses(engineClass, manifoldClass);
    }

    if (v.contains("carburetor_mounts")) {
        const auto mounts = toml::find<std::vector<int>>(v, "carburetor_mounts");
        for (int i = 0; i < kMaxCarburetors; i++) {
            if (std::find(mounts.begin(), mounts.end(), i) == mounts.end()) car->RemoveCarburetor(i);
        }
    }
    if (!toml::find_or(v, "engine_fitted", true)) car->RemoveEngine();
    if (!toml::find_or(v, "manifold_fitted", true)) car->RemoveManifold();
    if (!toml::find_or(v, "gearbox_fitted", true)) car->RemoveGearbox();
    car->SetSr2Parts(toml::find_or(v, "exhaust", car->GetExhaust()),
                     toml::find_or(v, "muffler", car->GetMuffler()),
                     toml::find_or(v, "differential", car->GetDifferential()));
    car->SetMufflerRemoved(toml::find_or(v, "muffler_removed", false));
    car->SetValue(toml::find_or(v, "value", model.price));
    return car;
}

template <typename PartT, typename TypeOf>
SaveValue SparesToToml(const std::vector<PartT>& parts, TypeOf typeOf) {
    SaveArray arr;
    for (const PartT& p : parts) arr.push_back(PartToToml(p, typeOf(p)));
    return SaveValue(arr);
}

template <typename PartT, typename FromToml>
void ReadSpares(const SaveValue& parent, const std::string& key,
                std::vector<PartT>& out, FromToml fromToml) {
    out.clear();
    if (!parent.contains(key)) return;
    for (const SaveValue& v : toml::find<SaveArray>(parent, key)) out.push_back(fromToml(v));
}

}

SaveSummary PeekSave(const fs::path& path) {
    std::ifstream f(path);
    if (!f) throw SaveLoadError("cannot open " + path.string());
    std::ostringstream buf;
    buf << f.rdbuf();

    const SaveValue root = ParseDocument(buf.str());
    CheckVersion(root);

    SaveSummary s;
    s.playerName   = toml::find_or(root, "name", std::string());
    s.saveName     = toml::find_or(root, "save_name", s.playerName);
    s.game         = toml::find_or(root, "game", std::string());
    s.bankroll     = toml::find_or(root, "bankroll", 0);
    s.elapsedTicks = static_cast<uint32_t>(toml::find_or(root, "elapsed_ticks", std::int64_t{0}));
    return s;
}

std::vector<fs::path> ListSaveFiles(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".toml") out.push_back(it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool State::SaveGameToml(const fs::path& path) {
    SaveValue root;
    root["version"]  = kSaveVersion;
    root["game"]     = std::string(SrGameSaveKey(assets.GetGame()));
    root["name"]     = playerName;
    root["save_name"] = GetSaveName();
    root["bankroll"] = bankroll;
    root["elapsed_ticks"] = static_cast<std::int64_t>(elapsedTicks);
    root["track_variant"] = GetTrackVariant();

    // Every model_index resolves through the spec table these sets produce.
    std::vector<std::string> enabledSets;
    const std::vector<CarSetDef>& saveDefs = GetCarSetDefs();
    for (size_t i = 0; i < saveDefs.size(); ++i) {
        if (assets.IsCarSetEnabled(static_cast<int>(i))) {
            enabledSets.push_back(saveDefs[i].id);
        }
    }
    root["car_sets"] = enabledSets;

    SaveValue rep;
    rep["drag_rep"]        = reputation.drag_rep;
    rep["road_rep"]        = reputation.road_rep;
    rep["races_completed"] = reputation.races_completed;
    rep["drag_wins"]       = reputation.drag_wins;
    rep["road_wins"]       = reputation.road_wins;
    rep["aqueduct_rep"]    = reputation.aqueduct_rep;
    rep["grudge_won"]      = reputation.grudge_won;
    rep["king_stage"]      = reputation.king_stage;
    rep["king_qualified"]  = kingQualifierWon;
    root["reputation"] = rep;

    SaveArray cars;
    int currentIndex = -1;
    for (const CarPtr& c : ownedCars) {
        if (!c) continue;
        if (c == currentCar) currentIndex = static_cast<int>(cars.size());
        cars.push_back(CarToToml(*c));
    }
    root["current_car"] = currentIndex;
    root["car"] = cars;

    SaveValue spares;
    spares["tires"] = SparesToToml(spareTires,
        [](const Part& p) { return static_cast<int>(p.partType.tireType); });
    spares["transmissions"] = SparesToToml(spareTransmissions,
        [](const Part& p) { return static_cast<int>(p.partType.transmissionType); });
    spares["manifolds"] = SparesToToml(spareManifolds,
        [](const Part& p) { return static_cast<int>(p.partType.manifoldType); });
    spares["carburetors"] = SparesToToml(spareCarburetors,
        [](const Part& p) { return static_cast<int>(p.partType.carburetorType); });
    SaveArray engines;
    for (const EnginePart& e : spareEngines) engines.push_back(EngineToToml(e));
    spares["engines"] = SaveValue(engines);
    auto sr2Class = [](const Part& p) { return p.partType.sr2Class; };
    spares["exhausts"]      = SparesToToml(spareExhausts, sr2Class);
    spares["mufflers"]      = SparesToToml(spareMufflers, sr2Class);
    spares["differentials"] = SparesToToml(spareDifferentials, sr2Class);
    root["spares"] = spares;

    // [driver_index, car_spec_index, status, color_index, body_byte, downgraded, races, opp_wins].
    SaveArray racers;
    for (const RacerEntry& e : racerPool.pool) {
        racers.push_back(SaveArray{e.driver_index, e.car_spec_index,
                                   static_cast<int>(e.status),
                                   static_cast<int>(e.color_index),
                                   static_cast<int>(e.body_byte),
                                   e.downgraded ? 1 : 0,
                                   static_cast<int>(e.races),
                                   static_cast<int>(e.opp_wins)});
    }
    SaveValue pool;
    pool["racers"] = racers;
    pool["last_challenger"] = lastChallengerPoolIndex;
    root["racer_pool"] = pool;

    std::vector<std::string> known;
    for (const Opponent* opp : GetKnownOpponents()) known.push_back(opp->driver.name);
    root["known_opponents"] = known;

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) {
        LOG_INFO("SaveGameToml: cannot write %s", path.string().c_str());
        return false;
    }
    f << toml::format(root);
    if (!f.good()) {
        LOG_INFO("SaveGameToml: write failed for %s", path.string().c_str());
        return false;
    }
    LOG_INFO("SaveGameToml: wrote %s", path.string().c_str());
    return true;
}

bool State::LoadGameToml(const fs::path& path) {
    SaveValue root;
    try {
        std::ifstream f(path);
        if (!f) throw SaveLoadError("cannot open " + path.string());
        std::ostringstream buf;
        buf << f.rdbuf();
        root = ParseDocument(buf.str());
        CheckVersion(root);
        CheckGame(root, assets.GetGame());
    } catch (const std::exception& e) {
        LOG_INFO("LoadGameToml failed: %s", e.what());
        return false;
    }

    // Before any car is resolved: a set's models only exist once RebuildCarSets has read it.
    if (root.contains("car_sets")) {
        const std::vector<std::string> ids =
            toml::find_or(root, "car_sets", std::vector<std::string>{});
        const std::vector<CarSetDef>& defs = GetCarSetDefs();
        for (size_t i = 0; i < defs.size(); ++i) {
            if (CarSetIsOwnGame(defs[i], assets.GetGame())) continue;
            const bool wanted = std::find(ids.begin(), ids.end(), defs[i].id) != ids.end();
            if (wanted && !assets.HasCarSetData(static_cast<int>(i))) {
                LOG_WARN("Save wants car set '%s' but its data is not installed; "
                         "its cars will not load", defs[i].id.c_str());
            }
            assets.SetCarSetEnabled(static_cast<int>(i), wanted);
        }
        RebuildCarSets();
    }

    // Only reads the document, so a malformed field throws before State is touched.
    std::vector<CarPtr> rebuilt;
    int currentIndex = -1;
    try {
        if (root.contains("car")) {
            for (const SaveValue& v : toml::find<SaveArray>(root, "car")) {
                rebuilt.push_back(CarFromToml(v, carData));
            }
        }
        currentIndex = toml::find_or(root, "current_car", -1);
    } catch (const std::exception& e) {
        LOG_INFO("LoadGameToml failed reading cars: %s", e.what());
        return false;
    }

    SetPlayerName(toml::find_or(root, "name", std::string()));
    SetSaveName(toml::find_or(root, "save_name", std::string()));
    SetBankroll(toml::find_or(root, "bankroll", 0));
    SetElapsedTicks(static_cast<uint32_t>(toml::find_or(root, "elapsed_ticks", std::int64_t{0})));
    SetTrackVariant(toml::find_or(root, "track_variant", 0));

    reputation = Reputation();
    kingQualifierWon = false;
    if (root.contains("reputation")) {
        const SaveValue& rep = toml::find(root, "reputation");
        reputation.drag_rep        = toml::find_or(rep, "drag_rep", 0);
        reputation.road_rep        = toml::find_or(rep, "road_rep", 0);
        reputation.races_completed = toml::find_or(rep, "races_completed", 0);
        reputation.drag_wins       = toml::find_or(rep, "drag_wins", 0);
        reputation.road_wins       = toml::find_or(rep, "road_wins", 0);
        reputation.aqueduct_rep    = toml::find_or(rep, "aqueduct_rep", 0);
        reputation.grudge_won      = toml::find_or(rep, "grudge_won", 0);
        reputation.king_stage      = toml::find_or(rep, "king_stage", 0);
        kingQualifierWon           = toml::find_or(rep, "king_qualified", false);
    }

    ownedCars = std::move(rebuilt);
    currentCar.reset();
    if (currentIndex >= 0 && currentIndex < static_cast<int>(ownedCars.size())) {
        currentCar = ownedCars.at(currentIndex);
    } else if (!ownedCars.empty()) {
        currentCar = ownedCars.at(0);
    }

    ClearSpares();
    if (root.contains("spares")) {
        const SaveValue& sp = toml::find(root, "spares");
        ReadSpares(sp, "tires",         spareTires,         TireFromToml);
        ReadSpares(sp, "transmissions", spareTransmissions, TransmissionFromToml);
        ReadSpares(sp, "manifolds",     spareManifolds,     ManifoldFromToml);
        ReadSpares(sp, "carburetors",   spareCarburetors,   CarburetorFromToml);
        ReadSpares(sp, "engines",       spareEngines,       EngineFromToml);
        ReadSpares(sp, "exhausts",      spareExhausts,      Sr2PartFromToml);
        ReadSpares(sp, "mufflers",      spareMufflers,      Sr2PartFromToml);
        ReadSpares(sp, "differentials", spareDifferentials, Sr2PartFromToml);
    }

    if (root.contains("racer_pool")) {
        const SaveValue& pool = toml::find(root, "racer_pool");
        lastChallengerPoolIndex = toml::find_or(pool, "last_challenger", -1);
        if (pool.contains("racers")) {
            const SaveArray racers = toml::find<SaveArray>(pool, "racers");
            for (size_t i = 0; i < racers.size() && i < racerPool.pool.size(); ++i) {
                const auto row = toml::get<std::vector<int>>(racers[i]);
                if (row.size() < 8) continue;
                racerPool.pool[i].driver_index   = row[0];
                racerPool.pool[i].car_spec_index = row[1];
                racerPool.pool[i].status         = static_cast<int8_t>(row[2]);
                racerPool.pool[i].color_index    = static_cast<uint8_t>(row[3]);
                racerPool.pool[i].body_byte      = static_cast<uint8_t>(row[4]);
                racerPool.pool[i].downgraded     = row[5] != 0;
                racerPool.pool[i].races          = static_cast<uint8_t>(row[6]);
                racerPool.pool[i].opp_wins       = static_cast<uint8_t>(row[7]);
            }
        }
    }

    GiveOpponentsPoolCars();

    SetKnownOpponentNames(toml::find_or(root, "known_opponents", std::vector<std::string>{}));

    // The ads are not saved.
    GenerateClassifieds();

    LOG_INFO("LoadGameToml: loaded '%s', bankroll=%d, %d owned car(s)",
             playerName.c_str(), bankroll, (int)ownedCars.size());
    return true;
}

namespace {

struct Snapshot {
    std::string text;
    bool operator==(const Snapshot& o) const { return text == o.text; }
};

Snapshot TakeSnapshot() {
    std::ostringstream o;
    o << "name=" << state.GetPlayerName()
      << " bankroll=" << state.GetBankroll()
      << " ticks=" << state.GetElapsedTicks()
      << " date=" << state.GetGameDateString();

    const Reputation& r = state.GetReputation();
    o << " rep=" << r.drag_rep << "/" << r.road_rep << "/" << r.races_completed
      << "/" << r.drag_wins << "/" << r.road_wins;

    for (const CarPtr& c : state.GetOwnedCars()) {
        o << "\n  car model=" << c->GetModelRef().index
          << " color=" << (int)c->GetColor()
          << " decal=" << (int)c->GetDecal()
          << " tune=" << c->GetTuneLevel()
          << " fuel=" << c->GetFuelTenths()
          << " weight=" << c->GetWeightClass()
          << " specE=" << c->GetSpecEngineClass()
          << " specM=" << c->GetSpecManifoldClass()
          << " roof=" << c->IsRoofChopped()
          << " bumpers=" << c->IsBumpersStripped()
          << " decalpath=" << c->GetCustomDecalPath()
          << " top=" << c->GetTopSpeed()
          << " eng=" << (int)c->GetEngine().engineType << "," << c->GetEngine().wear
          << "," << c->GetEngine().cost << "," << (int)c->GetEngine().vendor
          << " trans=" << (int)c->GetGearbox().partType.transmissionType
          << "," << c->GetGearbox().wear << "," << c->GetGearbox().cost
          << " manif=" << (int)c->GetManifold().partType.manifoldType
          << " carb=" << (int)c->GetCarburator().partType.carburetorType
          << " tires=" << (int)c->GetTires().partType.tireType
          << "," << c->GetTires().wear << "," << c->GetTires().cost
          << " name=" << c->GetGearbox().name;
    }
    o << "\n  current=" << (state.GetCurrentCar() ? state.GetCurrentCar()->GetModelRef().index : -1);

    auto spares = [&o](const char* tag, const std::vector<Part>& v, int (*typeOf)(const Part&)) {
        o << "\n  " << tag << "=";
        for (const Part& p : v) o << typeOf(p) << "," << p.wear << "," << p.cost << "," << p.name << ";";
    };
    spares("tires", state.GetSpareTires(),
           [](const Part& p) { return (int)p.partType.tireType; });
    spares("trans", state.GetSpareTransmissions(),
           [](const Part& p) { return (int)p.partType.transmissionType; });
    spares("manif", state.GetSpareManifolds(),
           [](const Part& p) { return (int)p.partType.manifoldType; });
    spares("carbs", state.GetSpareCarburetors(),
           [](const Part& p) { return (int)p.partType.carburetorType; });
    o << "\n  engines=";
    for (const EnginePart& e : state.GetSpareEngines()) {
        o << (int)e.engineType << "," << e.wear << "," << e.cost << "," << e.name << ";";
    }

    o << "\n  known=";
    for (const Opponent* opp : state.GetKnownOpponents()) o << opp->driver.name << ";";

    o << "\n  pool=";
    for (const RacerEntry& e : state.GetRacerPool().pool) {
        o << e.driver_index << "/" << e.car_spec_index << "/" << (int)e.status
          << "/" << (int)e.color_index << "/" << (int)e.body_byte
          << "/" << (int)e.downgraded << "/" << (int)e.races
          << "/" << (int)e.opp_wins << ";";
    }
    return Snapshot{o.str()};
}

// Non-default values everywhere, so a dropped field shows as a mismatch.
void ScrambleState() {
    state.SetPlayerName("SELFTEST");
    state.SetBankroll(4242);
    state.SetElapsedTicks(90000);
    Reputation& r = state.GetReputation();
    r.drag_rep = 5; r.road_rep = 2; r.races_completed = 9;
    r.drag_wins = 6; r.road_wins = 1;

    if (Car* c = state.GetCurrentCar()) {
        c->SetTuneLevel(12);
        c->SetFuelTenths(77);
        c->SetColor(static_cast<StandardCarColor>(3));
        c->SetRoofChopped(true);
        c->SetTires(RACING, 3317, 210);
        Part trans = CarBuilder::CreateTransmissionPart(TRANS_4SPD_UNIVERSAL);
        trans.wear = 1737; trans.cost = 555; trans.name = "Trnsmsn 4-spd. Ford.";
        c->SetGearbox(trans);
        c->SetEngine(EnginePart(CarEngineFord_V8_351, 1183, 800, CarVendorFord, ENGINE_V8_BIG));
    }
    state.AddSpareTire(Part(407, 60, CarVendorGM, TireAssetForType(BRAND), BRAND));
    state.AddSpareEngine(EnginePart(CarEngineFord_V8_351, 1183, 800, CarVendorFord, ENGINE_V8_BIG));
    if (!state.GetRacerPool().pool.empty()) {
        state.AddKnownOpponent(state.GiveOpponentPoolCar(state.GetRacerPool().pool[2]));
        state.AddKnownOpponent(state.GiveOpponentPoolCar(state.GetRacerPool().pool[5]));
        state.GetRacerPool().pool[3].status = -1;
        state.GetRacerPool().pool[4].downgraded = true;
        state.GetRacerPool().pool[5].races = 12;
        state.GetRacerPool().pool[5].opp_wins = 7;
    }
}

}

bool RunSaveRoundTripSelfTest() {
    ScrambleState();
    const Snapshot before = TakeSnapshot();

    const fs::path path = fs::temp_directory_path() / "rodlegacy_selftest.toml";
    if (!state.SaveGameToml(path)) {
        LOG_ERROR("selftest: save FAILED");
        return false;
    }
    state.SetKnownOpponentNames({});
    if (!state.LoadGameToml(path)) {
        LOG_ERROR("selftest: load FAILED");
        return false;
    }
    const Snapshot after = TakeSnapshot();
    std::error_code ec;
    fs::remove(path, ec);

    if (before == after) {
        LOG_INFO("selftest: save/load round trip OK (%d cars)",
                 (int)state.GetOwnedCars().size());
        return true;
    }
    LOG_ERROR("selftest: round trip MISMATCH");
    LOG_ERROR("--- before ---\n%s", before.text.c_str());
    LOG_ERROR("--- after  ---\n%s", after.text.c_str());
    return false;
}
