#include "bet_acceptance.h"

// Threshold table, big-endian int16 x 4x3 (24 bytes), at file offset 0x74C in
// the Amiga SR_data asset. Confirmed live: Amiga runtime memory at FUN_0023d718
// reads this table via `LEA (-$2B64,A4),A0` (A4-relative table base), and a
// runtime dump of A4-0x2B64 decoded to these exact 12 values; the same bytes
// are present in SR_data at this file offset, verified against the actual
// disk2 asset.
constexpr size_t kThresholdTableOffset = 0x74C;

static int kThreshold[4][3] = {
    {40, 60, 90},  // NoMoney
    {70, 65, 25},  // SmallMoney
    {90, 70, 15},  // LargeMoney
    {90, 35,  0},  // PinkSlips (threshold 0: never accepts when player is stronger)
};

void LoadBetThresholdTable(const std::vector<uint8_t>& srData) {
    if (srData.size() < kThresholdTableOffset + 24) {
        return;
    }
    const uint8_t* p = srData.data() + kThresholdTableOffset;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            const size_t i = static_cast<size_t>(row) * 3 + col;
            kThreshold[row][col] = (p[i * 2] << 8) | p[i * 2 + 1];
        }
    }
}

// Map a performance score (0-130) to a tier for relative-strength comparison.
static int ScoreToTier(int score) {
    if (score < 90)  return 0;  // weak
    if (score < 110) return 1;  // mid
    return 2;                   // strong
}

bool OpponentAcceptsBet(int playerScore, int opponentScore, BetType bet, int roll) {
    int pt = ScoreToTier(playerScore);
    int ot = ScoreToTier(opponentScore);

    int rel;
    if (pt > ot)      rel = 2;  // player stronger
    else if (pt < ot) rel = 0;  // player weaker
    else              rel = 1;  // equal

    return roll <= kThreshold[static_cast<int>(bet)][rel];
}
