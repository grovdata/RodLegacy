#include <doctest/doctest.h>
#include "race/bet_acceptance.h"

// strength[carClassTier][engineClass], lower = faster:
//   210 180 150 / 175 150 125 / 140 120 100
// car class 0..2 = tier 0, 3..6 = tier 1, 7..8 = tier 2.
// handicap[driverTier][stakeClass]: 60 45 12 / 55 40 8 / 50 30 5, added to the
// opponent's value before comparing.

namespace {

// Two identical cars, easy driver, no sticker.
BetContext Even() {
    BetContext ctx;
    ctx.playerCarClass = 4;
    ctx.playerEngineClass = 1;
    ctx.oppCarClass = 4;
    ctx.oppEngineClass = 1;
    return ctx;
}

// Player in the slowest car in the game, opponent in the fastest.
BetContext PlayerWeaker() {
    BetContext ctx = Even();
    ctx.playerCarClass = 0;   // strength 210
    ctx.playerEngineClass = 0;
    ctx.oppCarClass = 8;      // strength 100, +12 handicap on slips = 112 < 210
    ctx.oppEngineClass = 2;
    return ctx;
}

BetContext PlayerStronger() {
    BetContext ctx = PlayerWeaker();
    std::swap(ctx.playerCarClass, ctx.oppCarClass);
    std::swap(ctx.playerEngineClass, ctx.oppEngineClass);
    return ctx;
}

}  // namespace

TEST_SUITE("OpponentAcceptsBet - relative strength") {

TEST_CASE("identical cars are evenly matched") {
    CHECK(OpponentAcceptsBet(Even(), BetType::PinkSlips, 35) == true);
    CHECK(OpponentAcceptsBet(Even(), BetType::PinkSlips, 36) == false);
}

TEST_CASE("player weaker: accepts slips up to 90") {
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 91) == false);
}

TEST_CASE("player stronger: never accepts slips") {
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::PinkSlips, 0) == true);  // 0 <= 0
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::PinkSlips, 1) == false);
}

TEST_CASE("money rows read the same columns") {
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::SmallMoney, 25) == true);
    CHECK(OpponentAcceptsBet(PlayerStronger(), BetType::SmallMoney, 26) == false);
    CHECK(OpponentAcceptsBet(Even(), BetType::LargeMoney, 70) == true);
    CHECK(OpponentAcceptsBet(Even(), BetType::LargeMoney, 71) == false);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::NoMoney, 40) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::NoMoney, 41) == false);
}

TEST_CASE("car class tiers at 3 and 7") {
    BetContext ctx = Even();
    ctx.playerCarClass = 2;  // tier 0 -> 180
    ctx.oppCarClass    = 3;  // tier 1 -> 150, +12 on slips = 162 < 180
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 90) == true);   // weaker col
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 91) == false);

    ctx.playerCarClass = 6;  // tier 1 -> 150
    ctx.oppCarClass    = 7;  // tier 2 -> 120, +12 = 132 < 150
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 91) == false);
}

}  // TEST_SUITE

TEST_SUITE("OpponentAcceptsBet - handicap") {

// The handicap is what makes a free race generous and a pink slip stingy: the
// same pairing reads "evenly matched" for kicks and "player stronger" for slips.
TEST_CASE("a wider free-race band covers a gap slips do not") {
    BetContext ctx = Even();
    ctx.playerCarClass = 4;     // 150
    ctx.playerEngineClass = 1;
    ctx.oppCarClass = 4;        // 125, so the player is 25 slower
    ctx.oppEngineClass = 2;

    // kicks: 125 + 60 = 185, not < 150, and 150 > 125 -> equal, threshold 60
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 60) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 61) == false);
    // slips: 125 + 12 = 137 < 150 -> player weaker, threshold 90
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 91) == false);
}

TEST_CASE("a harder driver hands out less handicap") {
    BetContext ctx = Even();
    ctx.playerCarClass = 4;     // 150
    ctx.playerEngineClass = 1;
    ctx.oppCarClass = 4;        // 125
    ctx.oppEngineClass = 2;
    ctx.driverTier = 2;         // kicks handicap 50 instead of 60

    // 125 + 50 = 175, still not < 150 -> equal
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 60) == true);

    ctx.playerCarClass = 0;     // 210 vs 125 + 50 = 175 -> player weaker
    ctx.playerEngineClass = 0;
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 40) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::NoMoney, 41) == false);
}

}  // TEST_SUITE

TEST_SUITE("OpponentAcceptsBet - sticker bonus") {

TEST_CASE("odd sticker adds 4, even adds 6, none adds nothing") {
    BetContext ctx = Even();
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 36) == false);
    ctx.stickerIndex = 1;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 39) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 40) == false);
    ctx.stickerIndex = 2;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 41) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 42) == false);
}

TEST_CASE("the bonus can beat a zero threshold") {
    BetContext ctx = PlayerStronger();
    ctx.stickerIndex = 8;
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 6) == true);
    CHECK(OpponentAcceptsBet(ctx, BetType::PinkSlips, 7) == false);
}

}  // TEST_SUITE

TEST_SUITE("LoadBetTables") {

TEST_CASE("decodes big-endian words from offset 0x728") {
    std::vector<uint8_t> srData(0x728 + 60, 0);
    // Strength rows all 100 except tier 0 col 0, so Even() stays equal and a
    // class-0 car reads as the slow one; NoMoney row set to {1, 2, 3}.
    const uint8_t tables[60] = {
        0, 200, 0, 100, 0, 100,   // strength
        0, 100, 0, 100, 0, 100,
        0, 100, 0, 100, 0, 100,
        0,   0, 0,   0, 0,   0,   // handicap: none, so any gap counts
        0,   0, 0,   0, 0,   0,
        0,   0, 0,   0, 0,   0,
        0,   1, 0,   2, 0,   3,   // NoMoney
        0,  70, 0,  65, 0,  25,   // SmallMoney
        0,  90, 0,  70, 0,  15,   // LargeMoney
        0,  90, 0,  35, 0,   0,   // PinkSlips
    };
    std::copy(std::begin(tables), std::end(tables), srData.begin() + 0x728);
    LoadBetTables(srData);

    BetContext weaker = Even();
    weaker.playerCarClass = 0;     // 200 against the opponent's 100
    weaker.playerEngineClass = 0;
    CHECK(OpponentAcceptsBet(weaker, BetType::NoMoney, 1) == true);
    CHECK(OpponentAcceptsBet(weaker, BetType::NoMoney, 2) == false);
    CHECK(OpponentAcceptsBet(Even(), BetType::NoMoney, 2) == true);
    CHECK(OpponentAcceptsBet(Even(), BetType::NoMoney, 3) == false);

    // Restore the shipped values so later suites see them.
    const uint8_t shipped[60] = {
        0, 210, 0, 180, 0, 150,
        0, 175, 0, 150, 0, 125,
        0, 140, 0, 120, 0, 100,
        0,  60, 0,  45, 0,  12,
        0,  55, 0,  40, 0,   8,
        0,  50, 0,  30, 0,   5,
        0,  40, 0,  60, 0,  90,
        0,  70, 0,  65, 0,  25,
        0,  90, 0,  70, 0,  15,
        0,  90, 0,  35, 0,   0,
    };
    std::vector<uint8_t> defaults(0x728 + 60, 0);
    std::copy(std::begin(shipped), std::end(shipped), defaults.begin() + 0x728);
    LoadBetTables(defaults);
}

TEST_CASE("too-short buffer leaves the tables unchanged") {
    std::vector<uint8_t> tooShort(10, 0);
    LoadBetTables(tooShort);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(PlayerWeaker(), BetType::PinkSlips, 91) == false);
}

}  // TEST_SUITE
