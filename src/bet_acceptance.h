#pragma once

// Opponent bet acceptance logic, ported from Amiga FUN_0023d718.
//
// Threshold table (from runtime memory dump of original binary):
//   rows  = stake level: 0=no money, 1=small money, 2=large money, 3=pink slips
//   cols  = relative strength: 0=player weaker, 1=equal, 2=player stronger
//   value = max roll (0-99) that results in acceptance; roll <= threshold => accept
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

// Returns true if the opponent accepts the proposed bet.
//
// playerScore / opponentScore: comparable performance values (e.g. from
//   ComputeCarPerformanceScore), used only to determine relative strength.
// bet: stake level of the proposed bet.
// roll: caller-supplied random integer in [0, 99].
//
// Keeping `roll` as an explicit parameter makes this function pure and
// trivially unit-testable without mocking a random source.
bool OpponentAcceptsBet(int playerScore, int opponentScore, BetType bet, int roll);
