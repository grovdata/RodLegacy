#include <doctest/doctest.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "racer_pool.h"
#include "parsers/libp.h"

// ============================================================================
// Test fixtures
// ============================================================================

static SrDriverData MakeDriver(uint8_t tier, uint8_t reaction = 0x10,
                               uint8_t aggression = 0x08) {
    SrDriverData d{};
    d.tier       = tier;
    d.reaction   = reaction;
    d.aggression = aggression;
    d.reserved3  = 0;
    return d;
}

// Builds a spec the way SR_data stores one: a class byte plus the packed config
// word. See RodLegacyResearch/docs/formats/sr_data.md for the bit layout.
static SrCarSpecEntry MakeSpec(uint8_t car_class, int engine = 0,
                               int trans = 0, int carb = 0,
                               int tires = 0, int manifold = 0,
                               int brand = 0, int price = 500) {
    SrCarSpecEntry s{};
    s.car_class = car_class;
    s.price[0]  = static_cast<uint8_t>((price >> 8) & 0xff);
    s.price[1]  = static_cast<uint8_t>(price & 0xff);
    const int cfg = ((brand & 0x7) << 13) | ((tires & 0x3) << 10) |
                    ((trans & 0x3) << 8)  | ((manifold & 0x3) << 6) |
                    ((engine & 0x7) << 3) |  (carb & 0x7);
    s.config[0] = static_cast<uint8_t>((cfg >> 8) & 0xff);
    s.config[1] = static_cast<uint8_t>(cfg & 0xff);
    return s;
}

// 22 drivers: 7 tier-0, 7 tier-1, 7 tier-2, 1 King
static std::vector<SrDriverData> MakeDriverTable() {
    std::vector<SrDriverData> v;
    for (int i = 0; i < 7; ++i) v.push_back(MakeDriver(0));
    for (int i = 0; i < 7; ++i) v.push_back(MakeDriver(1));
    for (int i = 0; i < 7; ++i) v.push_back(MakeDriver(2));
    v.push_back(MakeDriver(2)); // King — index 21
    return v;
}

// 26 car specs. Tiering is car_class/3, so classes 0/3/6 land in tiers 0/1/2.
static std::vector<SrCarSpecEntry> MakeCarSpecTable() {
    std::vector<SrCarSpecEntry> v;
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(0));  // tier 0
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(3));  // tier 1
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(6));  // tier 2
    for (int i = 0; i < 4; ++i) v.push_back(MakeSpec(8));  // tier 2 spares
    // The King, index 25: fully specced, and never for sale (price 0).
    v.push_back(MakeSpec(8, /*engine*/4, /*trans*/3, /*carb*/2, /*tires*/2,
                         /*manifold*/2, /*brand*/0, /*price*/0));
    return v;
}

// ============================================================================
// ComputeCarPerformanceScore
// ============================================================================

TEST_SUITE("ComputeCarPerformanceScore") {

// Scoring goes through srrace::ComputeBasePerformance (the FUN_002435f0 port),
// so these assert behaviour - ordering, bounds, round-tripping - rather than
// pinning constants that would just restate the formula.

TEST_CASE("an all-stock spec scores the floor") {
    // A spec carries no weight or tune data, so it keeps CarSetup's defaults
    // (weightClass 4, tuneOffset 0) and scores above the bare 70 floor of an
    // all-worst setup. Assert the property rather than the number.
    const SrCarSpecEntry stock{};
    const int floorScore = ComputeCarPerformanceScore(stock);
    CHECK(floorScore >= 70);
    for (int engine = 0; engine < 8; ++engine) {
        for (int trans = 0; trans < 4; ++trans) {
            const auto better = MakeSpec(0, engine, trans, 2, 2, 3);
            CHECK(ComputeCarPerformanceScore(better) >= floorScore);
        }
    }
}

TEST_CASE("better parts score strictly higher, and the order is sane") {
    const int stock = ComputeCarPerformanceScore(MakeSpec(0));
    const int carb  = ComputeCarPerformanceScore(MakeSpec(0, 0, 0, 2));
    const int loaded = ComputeCarPerformanceScore(
        MakeSpec(8, /*engine*/1, /*trans*/3, /*carb*/2, /*tires*/2, /*manifold*/3));
    CHECK(stock < carb);
    CHECK(carb <= loaded);
    CHECK(loaded > stock);
}

TEST_CASE("score stays within the original's 0..130 range") {
    // Every field at its widest, including values the config word can hold but
    // CarSetup clamps (engine 7, manifold 3).
    for (int engine = 0; engine < 8; ++engine) {
        for (int trans = 0; trans < 4; ++trans) {
            for (int manifold = 0; manifold < 4; ++manifold) {
                const auto spec = MakeSpec(8, engine, trans, 2, 2, manifold);
                const int score = ComputeCarPerformanceScore(spec);
                CHECK(score >= 0);
                CHECK(score <= 130);
            }
        }
    }
}

TEST_CASE("config word packs and unpacks through the accessors") {
    const auto spec = MakeSpec(5, /*engine*/5, /*trans*/2, /*carb*/1,
                               /*tires*/2, /*manifold*/3, /*brand*/2,
                               /*price*/1234);
    CHECK(spec.car_class == 5);
    CHECK(spec.Price() == 1234);
    CHECK(spec.ForSale());
    CHECK(spec.EngineClass() == 5);
    CHECK(spec.TransClass() == 2);
    CHECK(spec.CarbClass() == 1);
    CHECK(spec.TireClass() == 2);
    CHECK(spec.ManifoldClass() == 3);
    CHECK(spec.Brand() == 2);
}

TEST_CASE("a price of zero means never for sale - only The King") {
    auto table = MakeCarSpecTable();
    REQUIRE(table.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_KING_CARSPEC_INDEX; ++i) {
        CHECK(table[i].ForSale());
    }
    CHECK_FALSE(table[SR_DATA_KING_CARSPEC_INDEX].ForSale());
}

} // TEST_SUITE

// ============================================================================
// RacerPool::Init
// ============================================================================

TEST_SUITE("RacerPool::Init") {

TEST_CASE("pool has RACER_POOL_SIZE entries after Init") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    CHECK(static_cast<int>(rp.pool.size()) == RACER_POOL_SIZE);
}

TEST_CASE("all entries are active (status=0) after Init") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    for (const auto& e : rp.pool) {
        CHECK(e.status == 0);
    }
}

TEST_CASE("CountActive returns 21 on fresh pool") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    CHECK(rp.CountActive() == 21);
}

TEST_CASE("no duplicate car_spec_index when enough specs are available") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    std::vector<int> seen;
    for (const auto& e : rp.pool) {
        CHECK(std::find(seen.begin(), seen.end(), e.car_spec_index) == seen.end());
        seen.push_back(e.car_spec_index);
    }
}

TEST_CASE("King driver index not in pool") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    for (const auto& e : rp.pool) {
        CHECK(e.driver_index != SR_DATA_KING_DRIVER_INDEX);
    }
}

} // TEST_SUITE

// ============================================================================
// RacerPool::CountActive / IsEliminated
// ============================================================================

TEST_SUITE("RacerPool::CountActive") {

TEST_CASE("eliminated entry reduces count") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[0].status = -1;
    CHECK(rp.CountActive() == 20);
}

TEST_CASE("IsEliminated returns true for status=-1") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[3].status = -1;
    CHECK(rp.IsEliminated(3) == true);
    CHECK(rp.IsEliminated(4) == false);
}

TEST_CASE("IsEliminated returns true for out-of-range index") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    CHECK(rp.IsEliminated(-1) == true);
    CHECK(rp.IsEliminated(RACER_POOL_SIZE) == true);
}

} // TEST_SUITE

// ============================================================================
// RacerPool::BuildSelectionPool
// ============================================================================

TEST_SUITE("RacerPool::BuildSelectionPool") {

TEST_CASE("returns at most SELECTION_POOL_SIZE indices") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    auto sel = rp.BuildSelectionPool(85, -1);
    CHECK(static_cast<int>(sel.size()) <= SELECTION_POOL_SIZE);
}

TEST_CASE("all indices are valid pool indices") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    auto sel = rp.BuildSelectionPool(85, -1);
    for (int idx : sel) {
        CHECK(idx >= 0);
        CHECK(idx < RACER_POOL_SIZE);
    }
}

TEST_CASE("eliminated entries are excluded") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[0].status = -1;
    auto sel = rp.BuildSelectionPool(85, -1);
    CHECK(std::find(sel.begin(), sel.end(), 0) == sel.end());
}

TEST_CASE("last_chosen_index is excluded when alternatives exist") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    // Pick first challenger, then ensure last_chosen is excluded from next pool.
    auto sel1 = rp.BuildSelectionPool(85, -1);
    REQUIRE(!sel1.empty());
    int chosen = sel1[0];
    auto sel2 = rp.BuildSelectionPool(85, chosen);
    if (sel2.size() > 0) {
        CHECK(std::find(sel2.begin(), sel2.end(), chosen) == sel2.end());
    }
}

TEST_CASE("last_chosen kept if only one active entry remains") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    // Eliminate all but one.
    for (int i = 1; i < RACER_POOL_SIZE; ++i) rp.pool[i].status = -1;
    auto sel = rp.BuildSelectionPool(85, 0);
    CHECK(sel.size() == 1);
    CHECK(sel[0] == 0);
}

} // TEST_SUITE

// ============================================================================
// RacerPool::OnRaceWon
// ============================================================================

TEST_SUITE("RacerPool::OnRaceWon") {

TEST_CASE("non-pink-slip win: entry stays active") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[0].status = 1; // currently challenging
    rp.OnRaceWon(0, false);
    CHECK(rp.pool[0].status == 0);
}

TEST_CASE("pink-slip win: entry eliminated when no lower-class car available") {
    // Give pool[0] a class-0 car — nothing lower exists.
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[0].car_spec_index = 0; // classification=0
    rp.pool[0].status = 1;
    rp.OnRaceWon(0, true);
    CHECK(rp.pool[0].status == -1);
}

TEST_CASE("pink-slip win: entry replaced with lower-class car when available") {
    // Give pool[0] a class-2 car; class-1 and class-0 cars are available.
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    // Find a class-2 spec index.
    int class2_spec = -1;
    auto specs = MakeCarSpecTable();
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        if (specs[i].car_class == 6) { class2_spec = i; break; }
    }
    REQUIRE(class2_spec >= 0);
    rp.pool[0].car_spec_index = class2_spec;
    rp.pool[0].status = 1;
    rp.OnRaceWon(0, true);
    CHECK(rp.pool[0].status == 0);  // stayed active
    CHECK(rp.pool[0].car_spec_index != class2_spec); // got a different car
}

TEST_CASE("pink-slip win: replaced car has lower classification") {
    RacerPool rp;
    auto specs = MakeCarSpecTable();
    rp.Init(MakeDriverTable(), specs);
    int class2_spec = -1;
    for (int i = 0; i < static_cast<int>(specs.size()); ++i) {
        if (specs[i].car_class == 6) { class2_spec = i; break; }
    }
    REQUIRE(class2_spec >= 0);
    rp.pool[0].car_spec_index = class2_spec;
    rp.pool[0].status = 1;
    rp.OnRaceWon(0, true);
    if (rp.pool[0].status == 0) {
        int new_spec = rp.pool[0].car_spec_index;
        CHECK(specs[new_spec].car_class < 6);
    }
}

TEST_CASE("out-of-range index is safely ignored") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    CHECK_NOTHROW(rp.OnRaceWon(-1, false));
    CHECK_NOTHROW(rp.OnRaceWon(RACER_POOL_SIZE, false));
}

} // TEST_SUITE

// ============================================================================
// RacerPool::OnRaceLost
// ============================================================================

TEST_SUITE("RacerPool::OnRaceLost") {

TEST_CASE("resets challenger status to active") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    rp.pool[5].status = 1;
    rp.OnRaceLost(5);
    CHECK(rp.pool[5].status == 0);
}

TEST_CASE("out-of-range index is safely ignored") {
    RacerPool rp;
    rp.Init(MakeDriverTable(), MakeCarSpecTable());
    CHECK_NOTHROW(rp.OnRaceLost(-1));
    CHECK_NOTHROW(rp.OnRaceLost(RACER_POOL_SIZE));
}

} // TEST_SUITE

// ============================================================================
// DataParser::ParseDriverData / ParseCarSpecTable
// ============================================================================

TEST_SUITE("DataParser::ParseDriverData") {

TEST_CASE("returns empty vector when data is too short") {
    DataParser parser;
    std::vector<uint8_t> too_short(10, 0);
    auto result = parser.ParseDriverData(too_short);
    CHECK(result.empty());
}

TEST_CASE("parses SR_DATA_DRIVER_ENTRY_COUNT entries from correctly sized buffer") {
    DataParser parser;
    const size_t block_size = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    std::vector<uint8_t> buf(block_size, 0);
    // Set entry 0: tier=1, reaction=0x14, aggression=0x08
    buf[0] = 1;
    buf[1] = 0x14;
    buf[2] = 0x08;
    auto result = parser.ParseDriverData(buf);
    REQUIRE(result.size() == SR_DATA_DRIVER_ENTRY_COUNT);
    CHECK(result[0].tier       == 1);
    CHECK(result[0].reaction   == 0x14);
    CHECK(result[0].aggression == 0x08);
}

TEST_CASE("all 22 entries are parsed") {
    DataParser parser;
    const size_t block_size = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    std::vector<uint8_t> buf(block_size, 0);
    // Set tier for each entry.
    for (int i = 0; i < SR_DATA_DRIVER_ENTRY_COUNT; ++i) {
        buf[i * sizeof(SrDriverData)] = static_cast<uint8_t>(i % 3);
    }
    auto result = parser.ParseDriverData(buf);
    REQUIRE(result.size() == SR_DATA_DRIVER_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_DRIVER_ENTRY_COUNT; ++i) {
        CHECK(result[i].tier == static_cast<uint8_t>(i % 3));
    }
}

} // TEST_SUITE

TEST_SUITE("DataParser::ParseCarSpecTable") {

TEST_CASE("returns empty vector when data is too short for both blocks") {
    DataParser parser;
    const size_t driver_block = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    std::vector<uint8_t> buf(driver_block, 0); // driver block only, no carspec block
    auto result = parser.ParseCarSpecTable(buf);
    CHECK(result.empty());
}

TEST_CASE("parses SR_DATA_CARSPEC_ENTRY_COUNT entries") {
    DataParser parser;
    const size_t driver_block  = SR_DATA_DRIVER_ENTRY_COUNT  * sizeof(SrDriverData);
    const size_t carspec_block = SR_DATA_CARSPEC_ENTRY_COUNT * sizeof(SrCarSpecEntry);
    std::vector<uint8_t> buf(driver_block + carspec_block, 0);
    // Set entry 0's class byte (offset +2 in the record) to 2.
    buf[driver_block + 2] = 2;
    auto result = parser.ParseCarSpecTable(buf);
    REQUIRE(result.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    CHECK(result[0].car_class == 2);
}

TEST_CASE("all 26 entries are parsed") {
    DataParser parser;
    const size_t driver_block  = SR_DATA_DRIVER_ENTRY_COUNT  * sizeof(SrDriverData);
    const size_t carspec_block = SR_DATA_CARSPEC_ENTRY_COUNT * sizeof(SrCarSpecEntry);
    std::vector<uint8_t> buf(driver_block + carspec_block, 0);
    for (int i = 0; i < SR_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        buf[driver_block + i * sizeof(SrCarSpecEntry) + 2] = static_cast<uint8_t>(i % 4);
    }
    auto result = parser.ParseCarSpecTable(buf);
    REQUIRE(result.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        CHECK(result[i].car_class == static_cast<uint8_t>(i % 4));
    }
}

} // TEST_SUITE

// ============================================================================
// SR_data driver names and portraits, against the shipped file
// ============================================================================

TEST_SUITE("SrDataDrivers") {

static std::vector<uint8_t> LoadRealSrData() {
    const char* paths[] = {
        "../RodLegacyBins/amiga/SR_disk2/SR_data",
        "../../RodLegacyBins/amiga/SR_disk2/SR_data",
    };
    for (const char* p : paths) {
        std::ifstream f(p, std::ios::binary);
        if (f) {
            return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                        std::istreambuf_iterator<char>());
        }
    }
    return {};
}

TEST_CASE("driver names and portraits come out of SR_data") {
    const std::vector<uint8_t> data = LoadRealSrData();
    if (data.empty()) {
        MESSAGE("shipped SR_data not found next to the build; skipping");
        return;
    }

    DataParser parser;
    const auto drivers = parser.ParseDriverData(data);
    const auto names = parser.ParseDriverNames(data);
    REQUIRE(drivers.size() == SR_DATA_DRIVER_ENTRY_COUNT);
    REQUIRE(names.size() == SR_DATA_DRIVER_ENTRY_COUNT);

    CHECK(names[0] == "Mike");
    CHECK(names[1] == "Ralph");
    // The two the old hardcoded list omitted, which shifted every later name.
    CHECK(names[12] == "Jim");
    CHECK(names[16] == "Tom");
    CHECK(names[SR_DATA_KING_DRIVER_INDEX] == "The King");

    // Portraits are a stored per-driver assignment, not a sequential run.
    CHECK(drivers[0].PortraitAsset() == 344);
    CHECK(drivers[1].PortraitAsset() == DRIVER_01);
    CHECK(drivers[SR_DATA_KING_DRIVER_INDEX].PortraitAsset() == DRIVER_THE_KING);

    // Every named driver's portrait is inside the driver sprite range.
    for (int i = 0; i < SR_DATA_KING_DRIVER_INDEX; ++i) {
        CHECK(drivers[i].PortraitAsset() >= DRIVER_01);
        CHECK(drivers[i].PortraitAsset() <= DRIVER_22);
    }
}

} // TEST_SUITE
