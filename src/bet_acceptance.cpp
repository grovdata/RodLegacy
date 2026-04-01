#include "bet_acceptance.h"

// Threshold table from Amiga runtime memory (FUN_0023d718, table at 0x00C39ADC).
// Row = stake level (BetType), Col = relative strength (0=weaker, 1=equal, 2=stronger).
// Acceptance condition: roll <= threshold, where roll is in [0, 99].
static const int kThreshold[4][3] = {
    {40, 60, 90},  // NoMoney
    {70, 65, 25},  // SmallMoney
    {90, 70, 15},  // LargeMoney
    {90, 35,  0},  // PinkSlips (threshold 0: never accepts when player is stronger)
};

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
