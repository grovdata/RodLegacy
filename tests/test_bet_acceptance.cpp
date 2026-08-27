#include <doctest/doctest.h>
#include "bet_acceptance.h"

// Score thresholds: <90 = weak (tier 0), 90-109 = mid (tier 1), >=110 = strong (tier 2).

TEST_SUITE("OpponentAcceptsBet - PinkSlips") {

TEST_CASE("player stronger: never accepts (threshold=0, roll=0 is boundary)") {
    // player tier 2 (score 110), opponent tier 0 (score 70) -> col=2, threshold=0
    // roll=0 -> 0 <= 0 -> true (edge case: exactly on boundary)
    CHECK(OpponentAcceptsBet(110, 70, BetType::PinkSlips, 0) == true);
}

TEST_CASE("player stronger: rejects for roll > 0") {
    CHECK(OpponentAcceptsBet(110, 70, BetType::PinkSlips, 1)  == false);
    CHECK(OpponentAcceptsBet(110, 70, BetType::PinkSlips, 50) == false);
    CHECK(OpponentAcceptsBet(110, 70, BetType::PinkSlips, 99) == false);
}

TEST_CASE("equal strength: accepts up to threshold 35") {
    // player tier 1 (score 95), opponent tier 1 (score 100) -> col=1, threshold=35
    CHECK(OpponentAcceptsBet(95, 100, BetType::PinkSlips, 0)  == true);
    CHECK(OpponentAcceptsBet(95, 100, BetType::PinkSlips, 35) == true);
    CHECK(OpponentAcceptsBet(95, 100, BetType::PinkSlips, 36) == false);
    CHECK(OpponentAcceptsBet(95, 100, BetType::PinkSlips, 99) == false);
}

TEST_CASE("player weaker: accepts up to threshold 90") {
    // player tier 0 (score 70), opponent tier 2 (score 120) -> col=0, threshold=90
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 0)  == true);
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 91) == false);
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 99) == false);
}

} // TEST_SUITE

TEST_SUITE("OpponentAcceptsBet - SmallMoney") {

TEST_CASE("player stronger: low chance (threshold=25)") {
    // player tier 2, opponent tier 0 -> col=2, threshold=25
    CHECK(OpponentAcceptsBet(115, 70, BetType::SmallMoney, 25) == true);
    CHECK(OpponentAcceptsBet(115, 70, BetType::SmallMoney, 26) == false);
}

TEST_CASE("equal strength: moderate chance (threshold=65)") {
    CHECK(OpponentAcceptsBet(95, 95, BetType::SmallMoney, 65) == true);
    CHECK(OpponentAcceptsBet(95, 95, BetType::SmallMoney, 66) == false);
}

TEST_CASE("player weaker: high chance (threshold=70)") {
    CHECK(OpponentAcceptsBet(70, 120, BetType::SmallMoney, 70) == true);
    CHECK(OpponentAcceptsBet(70, 120, BetType::SmallMoney, 71) == false);
}

} // TEST_SUITE

TEST_SUITE("OpponentAcceptsBet - LargeMoney") {

TEST_CASE("player stronger: very low chance (threshold=15)") {
    CHECK(OpponentAcceptsBet(115, 70, BetType::LargeMoney, 15) == true);
    CHECK(OpponentAcceptsBet(115, 70, BetType::LargeMoney, 16) == false);
}

TEST_CASE("equal strength: moderate chance (threshold=70)") {
    CHECK(OpponentAcceptsBet(95, 95, BetType::LargeMoney, 70) == true);
    CHECK(OpponentAcceptsBet(95, 95, BetType::LargeMoney, 71) == false);
}

TEST_CASE("player weaker: very high chance (threshold=90)") {
    CHECK(OpponentAcceptsBet(70, 120, BetType::LargeMoney, 90) == true);
    CHECK(OpponentAcceptsBet(70, 120, BetType::LargeMoney, 91) == false);
}

} // TEST_SUITE

TEST_SUITE("LoadBetThresholdTable") {

TEST_CASE("decodes big-endian words at offset 0x74C and rewires OpponentAcceptsBet") {
    std::vector<uint8_t> srData(0x74C + 24, 0);
    // NoMoney row set to {1, 2, 3} instead of the default {40, 60, 90}.
    const uint8_t table[24] = {
        0, 1, 0, 2, 0, 3,     // NoMoney
        0, 70, 0, 65, 0, 25,  // SmallMoney (unchanged)
        0, 90, 0, 70, 0, 15,  // LargeMoney (unchanged)
        0, 90, 0, 35, 0, 0,   // PinkSlips (unchanged)
    };
    std::copy(std::begin(table), std::end(table), srData.begin() + 0x74C);

    LoadBetThresholdTable(srData);
    CHECK(OpponentAcceptsBet(70, 120, BetType::NoMoney, 1) == true);   // weaker col, threshold 1
    CHECK(OpponentAcceptsBet(70, 120, BetType::NoMoney, 2) == false);
    CHECK(OpponentAcceptsBet(95, 95, BetType::NoMoney, 2) == true);    // equal col, threshold 2
    CHECK(OpponentAcceptsBet(95, 95, BetType::NoMoney, 3) == false);

    // Restore the default table so later test suites see the original values.
    std::vector<uint8_t> defaultSrData(0x74C + 24, 0);
    const uint8_t defaultTable[24] = {
        0, 40, 0, 60, 0, 90,
        0, 70, 0, 65, 0, 25,
        0, 90, 0, 70, 0, 15,
        0, 90, 0, 35, 0, 0,
    };
    std::copy(std::begin(defaultTable), std::end(defaultTable), defaultSrData.begin() + 0x74C);
    LoadBetThresholdTable(defaultSrData);
}

TEST_CASE("too-short buffer leaves the table unchanged") {
    std::vector<uint8_t> tooShort(10, 0);
    LoadBetThresholdTable(tooShort);
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 90) == true);
    CHECK(OpponentAcceptsBet(70, 120, BetType::PinkSlips, 91) == false);
}

} // TEST_SUITE

TEST_SUITE("OpponentAcceptsBet - tier boundary") {

TEST_CASE("score 89 is tier 0 (weak)") {
    // two weak-tier opponents -> equal (col=1), PinkSlips threshold=35
    CHECK(OpponentAcceptsBet(89, 89, BetType::PinkSlips, 35) == true);
    CHECK(OpponentAcceptsBet(89, 89, BetType::PinkSlips, 36) == false);
}

TEST_CASE("score 90 enters tier 1 (mid)") {
    // player 90 (tier1) vs opponent 89 (tier0) -> player stronger (col=2), threshold=0
    CHECK(OpponentAcceptsBet(90, 89, BetType::PinkSlips, 0) == true);
    CHECK(OpponentAcceptsBet(90, 89, BetType::PinkSlips, 1) == false);
}

TEST_CASE("score 110 enters tier 2 (strong)") {
    // player 110 (tier2) vs opponent 109 (tier1) -> player stronger
    CHECK(OpponentAcceptsBet(110, 109, BetType::PinkSlips, 0) == true);
    CHECK(OpponentAcceptsBet(110, 109, BetType::PinkSlips, 1) == false);
}

} // TEST_SUITE
