#include <doctest/doctest.h>
#include "race/bet_acceptance.h"

#include <utility>

// Invented tables, not the game's. Loaded through LoadBetTables, so the values
// over 255 also cover the big-endian decode.
//
// strength[carClassTier][engineClass], lower = faster:
//   300 280 260 / 240 220 200 / 180 160 140
// car class 0..2 = tier 0, 3..6 = tier 1, 7..8 = tier 2.
// handicap[driverTier][stakeClass]: 70 50 10 / 60 40 5 / 50 30 0
// threshold[bet][weaker, equal, stronger]:
//   NoMoney 45 55 85 / SmallMoney 75 60 30 / LargeMoney 95 65 20 / PinkSlips 80 30 4

namespace {

std::vector<uint8_t> TestSrData() {
    const int words[30] = {
        300, 280, 260, 240, 220, 200, 180, 160, 140,
        70, 50, 10, 60, 40, 5, 50, 30, 0,
        45, 55, 85, 75, 60, 30, 95, 65, 20, 80, 30, 4,
    };
    std::vector<uint8_t> data(0x728 + 60, 0);
    for (int i = 0; i < 30; ++i) {
        data[0x728 + i * 2]     = static_cast<uint8_t>(words[i] >> 8);
        data[0x728 + i * 2 + 1] = static_cast<uint8_t>(words[i] & 0xFF);
    }
    return data;
}

struct Tables {
    Tables() { LoadBetTables(TestSrData()); }
};

// Two identical cars, easy driver, no sticker.
BetContext Even() {
    BetContext ctx;
    ctx.playerCarClass = 4;
    ctx.playerEngineClass = 1;
    ctx.oppCarClass = 4;
    ctx.oppEngineClass = 1;
    return ctx;
}

// Player in the slowest car, opponent in the fastest.
BetContext PlayerWeaker() {
    BetContext ctx = Even();
    ctx.playerCarClass = 0;   // strength 300
    ctx.playerEngineClass = 0;
    ctx.oppCarClass = 8;      // strength 140, +10 handicap on slips = 150 < 300
    ctx.oppEngineClass = 2;
    return ctx;
}

BetContext PlayerStronger() {
    BetContext ctx = PlayerWeaker();
    std::swap(ctx.playerCarClass, ctx.oppCarClass);
    std::swap(ctx.playerEngineClass, ctx.oppEngineClass);
    return ctx;
}

}

TEST_SUITE("OpponentAcceptsBet - relative strength") {

TEST_CASE_FIXTURE(Tables, "identical cars are evenly matched") {
    CHECK(OpponentAcceptsBet(Even(), BetType::PinkSlips, 30) == true);
    CHECK(OpponentAcceptsBet(Even(), BetType::PinkSlips, 31) == false);
}

TEST_CASE_FIXTURE(Tables, "player weaker reads the weaker column") {
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 80) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 81) == false);
}

TEST_CASE_FIXTURE(Tables, "player stronger reads the stronger column") {
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::PinkSlips, 4) == true);
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::PinkSlips, 5) == false);
}

TEST_CASE_FIXTURE(Tables, "money rows read the same columns") {
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::SmallMoney, 30) == true);
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::SmallMoney, 31) == false);
    CHECK(OpponentAcceptsBet(Even(), BetType::LargeMoney, 65) == true);
    CHECK(OpponentAcceptsBet(Even(), BetType::LargeMoney, 66) == false);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::NoMoney, 45) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::NoMoney, 46) == false);
}

TEST_CASE_FIXTURE(Tables, "car class tiers at 3 and 7") {
    BetContext ctx = Even();
    ctx.playerCarClass = 2;  // tier 0 -> 280
    ctx.oppCarClass    = 3;  // tier 1 -> 220, +10 on slips = 230 < 280
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 80) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 81) == false);

    ctx.playerCarClass = 6;  // tier 1 -> 220
    ctx.oppCarClass    = 7;  // tier 2 -> 160, +10 = 170 < 220
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 80) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 81) == false);
}

}

TEST_SUITE("OpponentAcceptsBet - handicap") {

TEST_CASE_FIXTURE(Tables, "a wider free-race band covers a gap slips do not") {
    BetContext ctx = Even();
    ctx.playerCarClass = 4;     // 220
    ctx.playerEngineClass = 1;
    ctx.oppCarClass = 4;        // 200, so the player is 20 slower
    ctx.oppEngineClass = 2;

    // kicks: 200 + 70 = 270, not < 220, and 220 > 200 -> equal
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 55) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 56) == false);
    // slips: 200 + 10 = 210 < 220 -> player weaker
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 80) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 81) == false);
}

TEST_CASE_FIXTURE(Tables, "a harder driver hands out less handicap") {
    BetContext ctx = Even();
    ctx.playerCarClass = 0;     // 260
    ctx.playerEngineClass = 2;
    ctx.oppCarClass = 4;        // 200
    ctx.oppEngineClass = 2;

    // easy driver: 200 + 70 = 270, not < 260 -> equal
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 55) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 56) == false);

    // hard driver: 200 + 50 = 250 < 260 -> player weaker
    ctx.driverTier = 2;
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 45) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 46) == false);
}

}

TEST_SUITE("OpponentAcceptsBet - sticker bonus") {

TEST_CASE_FIXTURE(Tables, "odd sticker adds 4, even adds 6, none adds nothing") {
    BetContext ctx = Even();
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 31) == false);
    ctx.stickerIndex = 1;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 34) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 35) == false);
    ctx.stickerIndex = 2;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 36) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 37) == false);
}

TEST_CASE_FIXTURE(Tables, "the bonus lifts a small threshold") {
    BetContext ctx = PlayerStronger();
    ctx.stickerIndex = 8;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 10) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 11) == false);
}

}

// Rivalry: (opp_wins * 2 - races) * threshold / 4.

TEST_SUITE("OpponentAcceptsBet - rivalry") {

TEST_CASE_FIXTURE(Tables, "no history leaves the threshold alone") {
    BetContext ctx = Even();           // evenly matched slips: threshold 30
    CHECK(ctx.rivalryMargin == 0);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 30) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 31) == false);
}

TEST_CASE_FIXTURE(Tables, "a driver who keeps beating the player is readier to bet") {
    BetContext ctx = Even();
    ctx.rivalryMargin = 4;             // 4 * 30 / 4 = +30
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 60) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 61) == false);
}

TEST_CASE_FIXTURE(Tables, "a driver the player keeps beating starts refusing") {
    BetContext ctx = Even();
    ctx.rivalryMargin = -4;            // -4 * 30 = -120, +3 then >>2 = -30
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 0) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 1) == false);
}

// -1 * 30 = -30; (-30 + 3) >> 2 = -7, where a plain floor of -30 / 4 gives -8.
TEST_CASE_FIXTURE(Tables, "a negative term rounds toward zero, not down") {
    BetContext ctx = Even();
    ctx.rivalryMargin = -1;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 23) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 24) == false);
}

}

TEST_SUITE("LoadBetTables") {

TEST_CASE_FIXTURE(Tables, "too-short buffer leaves the tables unchanged") {
    std::vector<uint8_t> tooShort(10, 0);
    LoadBetTables(tooShort);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 80) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 81) == false);
}

}
