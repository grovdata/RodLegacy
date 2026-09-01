#include "car_builder.h"
#include "car.h"
#include "parsers/libp.h"
#include "car_system.h"
#include "utils.h"
#include <random>
#include <algorithm>

CarPtr CarBuilder::BuildCar(CarModel model) {
    CarPtr car = std::make_shared<Car>(model);

    // Set up proper engine matching the vendor
    CarEngine_t engineType = GetDefaultEngineForVendor(model.vendor);
    EnginePart engine(engineType, 0, 0, model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    return car;
}

CarPtr CarBuilder::BuildRandomCar(CarModel model) {
    CarPtr car = std::make_shared<Car>(model);

    // Select compatible engine
    CarEngine_t engineType = SelectCompatibleEngine(model);
    EnginePart engine(engineType, 0, 0, model.vendor, (AmigaLibAsset)engineType);
    car->SetEngine(engine);

    // Select compatible transmission
    TransmissionType transType = SelectCompatibleTransmission(engineType);
    Part transmission = CreateTransmissionPart(transType, model.vendor);
    car->SetGearbox(transmission);

    // Select compatible manifold
    ManifoldType manifoldType = SelectCompatibleManifold(engineType);
    Part manifold = CreateManifoldPart(manifoldType, model.vendor);
    car->SetManifold(manifold);

    // Select compatible carburetor - must fit the manifold just chosen, not
    // just the engine: a manifold's own carb fitment is stricter (e.g. a
    // 4-barrel manifold only takes a 4-barrel carb, per the newspaper
    // classifieds' own ad text - PartsCompatibilityManager::
    // GetCompatibleCarburetors(ManifoldType)). Picking carb from the engine
    // alone could pair a 4-barrel manifold with a 2-barrel carb on a V6.
    CarburetorType carbType = SelectCompatibleCarburetor(manifoldType);
    Part carburetor = CreateCarburetorPart(carbType, model.vendor);
    car->SetCarburator(carburetor);

    // Random tires
    std::uniform_int_distribution<> tireDist(0, (int)CarWheelType::RACING);
    car->SetTires((CarWheelType)tireDist(RandomEngine()));

    return car;
}

void CarBuilder::ApplySpec(Car* car, const SrCarSpecEntry& spec, CarVendor_t vendor) {
    if (!car) {
        return;
    }

    static const CarburetorType kCarbByClass[3] = {
        CARB_2BARREL, CARB_4BARREL, CARB_4BARREL_RACING
    };
    // Transmission class 0..3 = auto / 3-speed / 4-speed / racing, picked in
    // the car's own brand.
    static const TransmissionType kTransByClass[3][4] = {
        {TRANS_AUTO_GM,       TRANS_3SPD_GM,       TRANS_4SPD_GM,       TRANS_RACING_GM},
        {TRANS_AUTO_FORD,     TRANS_3SPD_FORD,     TRANS_4SPD_FORD,     TRANS_RACING_FORD},
        {TRANS_AUTO_CHRYSLER, TRANS_3SPD_CHRYSLER, TRANS_4SPD_CHRYSLER, TRANS_RACING_CHRYSLER},
    };
    static const CarWheelType kTiresByClass[3] = {STOCK, BRAND, RACING};

    int vendorRow = 0;
    switch (vendor) {
        case CarVendorFord: vendorRow = 1; break;
        case CarVendorChrysler: vendorRow = 2; break;
        default: vendorRow = 0; break;
    }

    const int carbClass = std::clamp(spec.CarbClass(), 0, 2);
    const int transClass = std::clamp(spec.TransClass(), 0, 3);
    const int tireClass = std::clamp(spec.TireClass(), 0, 2);

    car->SetCarburator(CreateCarburetorPart(kCarbByClass[carbClass], vendor));
    car->SetGearbox(CreateTransmissionPart(kTransByClass[vendorRow][transClass], vendor));
    car->SetTires(kTiresByClass[tireClass]);
    car->SetWeightClass(spec.car_class);

    // Engine and manifold go in as raw scoring classes rather than as parts:
    // the spec's manifold field runs 0..4 and RodLegacy's ManifoldType only has
    // three values, so a class-1 manifold (which half the shipped cars carry)
    // has nowhere to live. The Car keeps the class until the player fits a
    // different part. Must come last - SetManifold above would clear it.
    car->SetSpecPartClasses(std::clamp(spec.EngineClass(), 0, 3),
                            std::clamp(spec.ManifoldClass(), 0, 4));
}

CarEngine_t CarBuilder::GetDefaultEngineForVendor(CarVendor_t vendor) {
    switch (vendor) {
        case CarVendorGM:
            return CarEngineGM_V6;
        case CarVendorFord:
            return CarEngineFord_V6;
        case CarVendorChrysler:
            return CarEngineChrysler_V6;
        case CarVendorCustom:
            return CarEngineGM_V6; // Custom cars might use GM as base
        case CarVendorUnknown:
            return CarEngineGM_V6; // Unknown vendor defaults to GM
        default:
            return CarEngineGM_V6; // This should never happen
    }
}

CarEngine_t CarBuilder::SelectCompatibleEngine(const CarModel& model) {
    if (model.compatibleEngines.empty()) {
        return GetDefaultEngineForVendor(model.vendor);
    }

    std::uniform_int_distribution<> dist(0, model.compatibleEngines.size() - 1);
    return model.compatibleEngines[dist(RandomEngine())];
}

TransmissionType CarBuilder::SelectCompatibleTransmission(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleTransmissions(engine);
    if (compatible.empty()) {
        return TRANS_4SPD_UNIVERSAL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

ManifoldType CarBuilder::SelectCompatibleManifold(CarEngine_t engine) {
    auto compatible = PartsCompatibilityManager::GetCompatibleManifolds(engine);
    if (compatible.empty()) {
        return MANIFOLD_2BARREL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

CarburetorType CarBuilder::SelectCompatibleCarburetor(ManifoldType manifold) {
    auto compatible = PartsCompatibilityManager::GetCompatibleCarburetors(manifold);
    if (compatible.empty()) {
        return CARB_2BARREL; // Default fallback
    }

    std::uniform_int_distribution<> dist(0, compatible.size() - 1);
    return compatible[dist(RandomEngine())];
}

Part CarBuilder::CreateTransmissionPart(TransmissionType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case TRANS_AUTO_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_GM); break;
        case TRANS_AUTO_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_FORD); break;
        case TRANS_AUTO_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_AUTO_CHRYSLER); break;
        case TRANS_3SPD_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_GM); break;
        case TRANS_3SPD_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_FORD); break;
        case TRANS_3SPD_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_3SPD_CHRYSLER); break;
        case TRANS_4SPD_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_GM); break;
        case TRANS_4SPD_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_FORD); break;
        case TRANS_4SPD_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_CHRYSLER); break;
        case TRANS_RACING_GM: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_GM); break;
        case TRANS_RACING_FORD: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_FORD); break;
        case TRANS_RACING_CHRYSLER: assetId = static_cast<AmigaLibAsset>(SR_TRANS_RACING_CHRYSLER); break;
        case TRANS_4SPD_UNIVERSAL: assetId = static_cast<AmigaLibAsset>(SR_TRANS_4SPD_UNIVERSAL); break;
    }
    return Part(0, 0, vendor, assetId, type);
}

// Kept in sync with engine_bay.cpp's ManifoldBodyAsset - see its comment for
// how this mapping was reconstructed from the actual lib.p art (not
// Ghidra-confirmed).
Part CarBuilder::CreateManifoldPart(ManifoldType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case MANIFOLD_2BARREL: assetId = ENGINE_MANIFOLD4; break;
        case MANIFOLD_4BARREL: assetId = ENGINE_MANIFOLD3; break;
        case MANIFOLD_DUAL_QUAD: assetId = ENGINE_MANIFOLD1; break;
        case MANIFOLD_TRIPLE_2BARREL: assetId = ENGINE_MANIFOLD6; break;
        case MANIFOLD_RACING: assetId = ENGINE_MANIFOLD3; break;
    }
    return Part(0, 0, vendor, assetId, type);
}

Part CarBuilder::CreateCarburetorPart(CarburetorType type, CarVendor_t vendor) {
    AmigaLibAsset assetId;
    switch (type) {
        case CARB_2BARREL: assetId = ENGINE_CARB_2BARREL; break;
        case CARB_4BARREL: assetId = ENGINE_CARB_4BARREL; break;
        case CARB_4BARREL_RACING: assetId = ENGINE_CARB_4BARREL_RACING; break;
    }
    return Part(0, 0, vendor, assetId, type);
}

// Data-mined from every sample .sav in RodLegacyResearch/savegames/amiga/
// (RodLegacyResearch/docs/formats/save_game.md): the save's part "variant"
// byte is an index into one flat master parts table, not a per-category
// one. Transmissions occupy variant 9-21, in exactly the price-catalog
// order README_transmission.md already extracted from the binary (which
// matches libp.h's SrTransmissionType enum) - every (price, variant) pair
// across every sample save agrees with "variant - 9 = catalog index".
bool CarBuilder::TransmissionTypeFromSaveVariant(int variant, TransmissionType& out) {
    static constexpr TransmissionType kCatalogOrder[SR_TRANS_COUNT] = {
        TRANS_AUTO_GM, TRANS_AUTO_FORD, TRANS_AUTO_CHRYSLER,
        TRANS_3SPD_GM, TRANS_3SPD_FORD, TRANS_3SPD_CHRYSLER,
        TRANS_4SPD_GM, TRANS_4SPD_FORD, TRANS_4SPD_CHRYSLER, TRANS_4SPD_UNIVERSAL,
        TRANS_RACING_GM, TRANS_RACING_FORD, TRANS_RACING_CHRYSLER,
    };
    int catalogIndex = variant - 9;
    if (catalogIndex < 0 || catalogIndex >= SR_TRANS_COUNT) return false;
    out = kCatalogOrder[catalogIndex];
    return true;
}

// The full carb catalog (3 entries, variant 22-24) is now confirmed: it
// sits immediately after transmission's 9-21 block in the master table, in
// ascending price order matching CarburetorType's own order. A Corvette
// test save went dual racing carbs (24/$60 x2) -> single 2-barrel (22/$25)
// -> dual plain 4-barrel (23/$40 x2, explicitly "not racing" per the
// player), pinning all 3 directly.
bool CarBuilder::CarburetorTypeFromSaveVariant(int variant, CarburetorType& out) {
    switch (variant) {
        case 22: out = CARB_2BARREL; return true;
        case 23: out = CARB_4BARREL; return true;
        case 24: out = CARB_4BARREL_RACING; return true;
    }
    return false;
}

// 3 of the engine catalog's 9 variants directly confirmed, all GM: 0=V6,
// 3=V8 small-block, 6=V8 big-block - a stride of 3 across GM's own 3 tiers.
// The other 6 (Ford/Chrysler) are inferred, not save-verified, by reusing
// the vendor order the game's transmission catalog already confirms
// (README_transmission.md, extracted from the binary): GM, Ford, Chrysler,
// repeated per tier, with no exception across any of its 4 tiers. Applying
// that same order to engine's 3 tiers is what fills in the other 6 -
// variant now fully determines vendor, so the caller no longer needs to
// pass one in.
bool CarBuilder::EngineTypeFromSaveVariant(int variant, CarEngine_t& outType, AmigaLibAsset& outAsset) {
    switch (variant) {
        case 0: outType = CarEngineGM_V6; outAsset = ENGINE_V6; return true;
        case 1: outType = CarEngineFord_V6; outAsset = ENGINE_V6; return true;
        case 2: outType = CarEngineChrysler_V6; outAsset = ENGINE_V6; return true;
        case 3: outType = CarEngineGM_V8_282; outAsset = ENGINE_V8; return true;
        case 4: outType = CarEngineFord_V8_292; outAsset = ENGINE_V8; return true;
        case 5: outType = CarEngineChrysler_V8_285; outAsset = ENGINE_V8; return true;
        case 6: outType = CarEngineGM_V8_327; outAsset = ENGINE_V8_BIG; return true;
        case 7: outType = CarEngineFord_V8_351; outAsset = ENGINE_V8_BIG; return true;
        case 8: outType = CarEngineChrysler_V8_340; outAsset = ENGINE_V8_BIG; return true;
    }
    return false;
}

// Manifold's block starts right after carb's (25 = class 0, matching the
// carb/transmission pattern exactly). Only the GM column (25, 31, 34, 37)
// is save-verified, all on the same GM Corvette test save sequence: variant
// 25/$20 = 2-barrel, variant 31/$35 = triple-2bbl (only fits after
// swapping to a V6 - see PartsCompatibilityManager::GetCompatibleManifolds),
// variant 34/$35 = dual-quad (explicitly "not racing" per the player),
// variant 37/$60 = racing (the car's original manifold - a plain "4-brl, 2
// carbs" description without the "not racing" qualifier turned out to mean
// racing, not dual-quad; corrected after a follow-up save disambiguated
// it). MANIFOLD_4BARREL had no GM anchor at all until this table was
// widened to the full grid below.
//
// The Ford/Chrysler columns (everything else here) are INFERRED, not
// save-verified: newspaper.cpp's classifieds catalog lists manifolds as 5
// types x 3 vendors (GM, Ford, Chrysler) in that order, and every other
// save-format catalog block (transmission's variant-9 stride, engine's
// stride-3 GM/Ford/Chrysler grouping) uses the same vendor order per tier -
// applying that stride-3 pattern to the 4 confirmed GM anchors is what
// fills in the other 11. Before this widening, any save with a non-GM
// manifold (or any 4-barrel manifold at all) silently fell through to the
// MANIFOLD_2BARREL fallback in State::LoadSaveGame, which is what produced
// bogus manifold/carb mismatches on load - flag if a specific Ford/Chrysler
// entry below turns out wrong.
bool CarBuilder::ManifoldTypeFromSaveVariant(int variant, ManifoldType& out) {
    switch (variant) {
        case 25: case 26: case 27: out = MANIFOLD_2BARREL; return true;
        case 28: case 29: case 30: out = MANIFOLD_4BARREL; return true;
        case 31: case 32: case 33: out = MANIFOLD_TRIPLE_2BARREL; return true;
        case 34: case 35: case 36: out = MANIFOLD_DUAL_QUAD; return true;
        case 37: case 38: case 39: out = MANIFOLD_RACING; return true;
    }
    return false;
}

// All 3 CarWheelType values confirmed: variant 40/$50 = stock (a second,
// unrelated car's tires in the same save batch - cheapest of the three,
// consistent with STOCK being the base tier), variant 41/$80 = brand-name
// (switched to from racing on the main test car), variant 42/$150 = racing
// slicks (that same car's original tires, matching its spec's
// TireClass=2 - see SrCarSpecEntry::TireClass in libp.h).
bool CarBuilder::TireTypeFromSaveVariant(int variant, CarWheelType& out) {
    switch (variant) {
        case 40: out = STOCK; return true;
        case 41: out = BRAND; return true;
        case 42: out = RACING; return true;
    }
    return false;
}
