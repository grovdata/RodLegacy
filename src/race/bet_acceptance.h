/*
 * RodLegacy - Whether an opponent takes a bet: OpponentAcceptsBet and the SR_data strength, handicap and
 * threshold tables.
 *
 * Created: 2026-04-01
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>
#include <vector>

// Three big-endian int16 tables in SR_data from 0x728, loaded by LoadBetTables.
//
// 0x728 strength 3x3, rows = car-class tier, cols = engine class. Lower is faster.
// 0x73A handicap 3x3, rows = driver tier, cols = stake class. Added to the opponent's strength,
//       creating the "evenly matched" band.
// 0x74C thresholds 4x3, rows = stake level, cols = relative strength (weaker, equal, stronger).
//       The opponent accepts when roll (0-99) <= threshold.

enum class BetType {
    NoMoney    = 0,
    SmallMoney = 1,
    LargeMoney = 2,
    PinkSlips  = 3,
};

struct BetContext {
    int playerCarClass   = 4;  // SR_data car-spec class byte, 0..8
    int playerEngineClass = 0; // engine tier 0..2 (V6 / small V8 / big V8)
    int oppCarClass      = 4;
    int oppEngineClass   = 0;
    int driverTier       = 0;  // SrDriverData::tier, 0..2
    int stickerIndex     = 0;  // car body byte high nibble: 0 = none, 1..8
    // opp_wins * 2 - races against this driver. Positive when they lead the rivalry.
    int rivalryMargin    = 0;
};

// Leaves the tables unchanged if srData is too short.
void LoadBetTables(const std::vector<uint8_t>& srData);

// roll is in [0, 99].
bool OpponentAcceptsBet(const BetContext& ctx, BetType bet, int roll);
