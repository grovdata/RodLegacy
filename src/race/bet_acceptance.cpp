#include "race/bet_acceptance.h"

#include <algorithm>

// Strength, handicap and threshold tables, 0x12 bytes apart.
constexpr size_t kTableOffset = 0x728;
constexpr size_t kTableBytes  = 60;

static int kStrength[3][3] = {};
static int kHandicap[3][3] = {};
static int kThreshold[4][3] = {};

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

    // Rivalry: (opp_wins * 2 - races) * threshold / 4. The original divides by shifting,
    // so it adds 3 first to round a negative product toward zero.
    int rivalry = ctx.rivalryMargin * threshold;
    if (rivalry < 0) rivalry += 3;
    rivalry >>= 2;

    return roll <= bonus + rivalry + threshold;
}
