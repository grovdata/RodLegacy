#pragma once

#include <cstdint>
#include <vector>

// Opponent bet acceptance logic, ported from Amiga FUN_0023d718.
//
// Three tables, contiguous in the SR_data asset from offset 0x728, all
// big-endian int16. Loaded at runtime by LoadBetTables; the values below are
// the fallback used if that load fails.
//
// 0x728 strength 3x3, rows = car-class tier, cols = engine class. LOWER IS
//       FASTER - the number is roughly the quarter-mile the car is good for.
//         210 180 150 / 175 150 125 / 140 120 100
// 0x73A handicap 3x3, rows = driver tier, cols = stake class. Added to the
//       opponent's strength before comparing, so it is what creates the
//       "evenly matched" band - wide for a free race, narrow for pink slips.
//          60  45  12 /  55  40   8 /  50  30   5
// 0x74C thresholds 4x3, rows = stake level, cols = relative strength.
//       Max roll (0-99) that accepts; roll <= threshold => accept.
//
//        weaker  equal  stronger
// no $:    40     60      90
// small:   70     65      25
// large:   90     70      15
// slips:   90     35       0    <- opponent never accepts when player is stronger

enum class BetType {
    NoMoney    = 0,
    SmallMoney = 1,
    LargeMoney = 2,
    PinkSlips  = 3,
};

// Everything the original feeds the comparison. Plain ints so this stays
// SDL-free and unit-testable; State::GetBetContext fills it in.
struct BetContext {
    int playerCarClass   = 4;  // SR_data car-spec class byte, 0..8
    int playerEngineClass = 0; // engine tier 0..2 (V6 / small V8 / big V8)
    int oppCarClass      = 4;
    int oppEngineClass   = 0;
    int driverTier       = 0;  // SrDriverData::tier, 0..2
    int stickerIndex     = 0;  // car body byte high nibble: 0 = none, 1..8
    // opp_wins * 2 - races against this driver, from their pool record's
    // bytes 4 and 5. Positive when they are ahead on the rivalry.
    int rivalryMargin    = 0;
};

// Populates the three tables from the raw SR_data asset bytes
// (AssetManager::GetSrData()). Call once at startup, after assets are
// loaded. Leaves the built-in fallbacks in place if srData is too short.
void LoadBetTables(const std::vector<uint8_t>& srData);

// Returns true if the opponent accepts the proposed bet.
// roll is a caller-supplied random integer in [0, 99]; keeping it an explicit
// parameter makes this pure and trivially testable without a random source.
bool OpponentAcceptsBet(const BetContext& ctx, BetType bet, int roll);
