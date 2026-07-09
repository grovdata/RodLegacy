#include <doctest/doctest.h>
#include <algorithm>
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
    d.pad0       = 0;
    return d;
}

static SrCarSpecEntry MakeSpec(uint8_t classification, uint8_t engine = 0,
                               uint8_t trans = 0, uint8_t carb = 0,
                               uint8_t tires = 0, uint8_t condition = 0) {
    SrCarSpecEntry s{};
    s.street_cred = classification;
    s.engine         = engine;
    s.transmission   = trans;
    s.carburetor     = carb;
    s.tires          = tires;
    s.condition      = condition;
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

// 26 car specs: 7 class-0, 7 class-1, 7 class-2, 4 class-3, 1 King
static std::vector<SrCarSpecEntry> MakeCarSpecTable() {
    std::vector<SrCarSpecEntry> v;
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(0));
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(1));
    for (int i = 0; i < 7; ++i) v.push_back(MakeSpec(2));
    for (int i = 0; i < 4; ++i) v.push_back(MakeSpec(3));
    v.push_back(MakeSpec(3, 8, 12, 2, 2, 3)); // King — index 25
    return v;
}

// ============================================================================
// ComputeCarPerformanceScore
// ============================================================================

TEST_SUITE("ComputeCarPerformanceScore") {

TEST_CASE("all-zero spec: base score 70 + class_bonus[0]=0") {
    SrCarSpecEntry spec{};
    CHECK(ComputeCarPerformanceScore(spec) == 70);
}

TEST_CASE("classification 1 adds class bonus") {
    auto spec = MakeSpec(1); // class_bonus[1] = 15
    CHECK(ComputeCarPerformanceScore(spec) == 85);
}

TEST_CASE("classification 2 adds larger class bonus") {
    auto spec = MakeSpec(2); // class_bonus[2] = 30
    CHECK(ComputeCarPerformanceScore(spec) == 100);
}

TEST_CASE("score clamped at 130") {
    // engine=8(+10) trans=12(+15) carb=2(+10) tires=2(+10) cond=3 class=3(+45)
    // 70 + 10 + 15 + 10 + 10 + 3 + 45 = 163 → clamped to 130
    auto spec = MakeSpec(3, 8, 12, 2, 2, 3);
    CHECK(ComputeCarPerformanceScore(spec) == 130);
}

TEST_CASE("condition bytes add directly to score") {
    auto spec = MakeSpec(0, 0, 0, 0, 0, 3);
    CHECK(ComputeCarPerformanceScore(spec) == 73);
}

TEST_CASE("out-of-range indices do not access tables") {
    SrCarSpecEntry spec{};
    spec.engine       = 9;  // out of range for ENGINE_SCORE[9]
    spec.transmission = 13; // out of range for TRANS_SCORE[13]
    spec.carburetor   = 3;  // out of range for CARB_SCORE[3]
    spec.tires        = 3;  // out of range for TIRE_SCORE[3]
    spec.street_cred = 4; // out of range for CLASS_BONUS[4]
    // None of the additions should apply
    CHECK(ComputeCarPerformanceScore(spec) == 70);
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
        if (specs[i].street_cred == 2) { class2_spec = i; break; }
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
        if (specs[i].street_cred == 2) { class2_spec = i; break; }
    }
    REQUIRE(class2_spec >= 0);
    rp.pool[0].car_spec_index = class2_spec;
    rp.pool[0].status = 1;
    rp.OnRaceWon(0, true);
    if (rp.pool[0].status == 0) {
        int new_spec = rp.pool[0].car_spec_index;
        CHECK(specs[new_spec].street_cred < 2);
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
    // Set entry 0 classification to 2.
    buf[driver_block + 0] = 2;
    auto result = parser.ParseCarSpecTable(buf);
    REQUIRE(result.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    CHECK(result[0].street_cred == 2);
}

TEST_CASE("all 26 entries are parsed") {
    DataParser parser;
    const size_t driver_block  = SR_DATA_DRIVER_ENTRY_COUNT  * sizeof(SrDriverData);
    const size_t carspec_block = SR_DATA_CARSPEC_ENTRY_COUNT * sizeof(SrCarSpecEntry);
    std::vector<uint8_t> buf(driver_block + carspec_block, 0);
    for (int i = 0; i < SR_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        buf[driver_block + i * sizeof(SrCarSpecEntry)] = static_cast<uint8_t>(i % 4);
    }
    auto result = parser.ParseCarSpecTable(buf);
    REQUIRE(result.size() == SR_DATA_CARSPEC_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        CHECK(result[i].street_cred == static_cast<uint8_t>(i % 4));
    }
}

} // TEST_SUITE
