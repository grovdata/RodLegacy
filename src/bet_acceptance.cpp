#include "bet_acceptance.h"

#include <algorithm>

// Start of the three tables in SR_data, 60 bytes in total. Confirmed live: the
// Amiga runtime reads them A4-relative at -0x2B88 / -0x2B76 / -0x2B64, 0x12
// bytes apart, and a runtime dump of the threshold table decoded to the same
// bytes the asset carries at 0x74C.
constexpr size_t kTableOffset = 0x728;
constexpr size_t kTableBytes  = 60;

static int kStrength[3][3] = {
    {210, 180, 150},
    {175, 150, 125},
    {140, 120, 100},
};

static int kHandicap[3][3] = {
    {60, 45, 12},
    {55, 40,  8},
    {50, 30,  5},
};

static int kThreshold[4][3] = {
    {40, 60, 90},  // NoMoney
    {70, 65, 25},  // SmallMoney
    {90, 70, 15},  // LargeMoney
    {90, 35,  0},  // PinkSlips (threshold 0: never accepts when player is stronger)
};

static void ReadBE16(const uint8_t* p, int* dst, int count) {
    for (int i = 0; i < count; ++i) {
        dst[i] = (p[i * 2] << 8) | p[i * 2 + 1];
    }
}

void LoadBetTables(const std::vector<uint8_t>& srData) {
    if (srData.size() < kTableOffset + kTableBytes) {
        return;
    }
    const uint8_t* p = srData.data() + kTableOffset;
    ReadBE16(p,      &kStrength[0][0],  9);
    ReadBE16(p + 18, &kHandicap[0][0],  9);
    ReadBE16(p + 36, &kThreshold[0][0], 12);
}

// FUN_0023d006: the car-spec class byte collapsed to a strength-table row.
static int CarClassTier(int carClass) {
    if (carClass < 3) return 0;
    if (carClass < 7) return 1;
    return 2;
}

// Handicap column: 0 = just for kicks, 1 = money, 2 = pink slips.
static int StakeClass(BetType bet) {
    if (bet == BetType::NoMoney)   return 0;
    if (bet == BetType::PinkSlips) return 2;
    return 1;
}

bool OpponentAcceptsBet(const BetContext& ctx, BetType bet, int roll) {
    const int playerValue =
        kStrength[CarClassTier(ctx.playerCarClass)][std::clamp(ctx.playerEngineClass, 0, 2)];
    const int opponentValue =
        kStrength[CarClassTier(ctx.oppCarClass)][std::clamp(ctx.oppEngineClass, 0, 2)];
    const int handicap =
        kHandicap[std::clamp(ctx.driverTier, 0, 2)][StakeClass(bet)];

    // Lower strength value = faster car.
    int rel;
    if (opponentValue + handicap < playerValue) rel = 0;  // player weaker
    else if (playerValue < opponentValue)       rel = 2;  // player stronger
    else                                        rel = 1;  // evenly matched

    const int threshold = kThreshold[static_cast<int>(bet)][rel];
    const int bonus = ctx.stickerIndex == 0 ? 0 : (ctx.stickerIndex & 1) ? 4 : 6;

    // Skipped: the original also adds (record[5] * 2 - record[4]) * threshold / 4
    // from the challenger's 8-byte pool record. Both bytes are 0 in every
    // observed Amiga save (save_game.md, chunk 6) and RodLegacy keeps no such
    // counters, so the term is always 0.
    return roll <= bonus + threshold;
}
