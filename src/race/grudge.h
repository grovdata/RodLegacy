#pragma once
// Grudge Night, Street Rod 2 only.

#include <cstdint>

#include "race/race_types.h"

namespace srrace {

// The meeting runs on Wednesdays and nothing else.
constexpr int GRUDGE_DAYS_PER_WEEK = 7;
constexpr int GRUDGE_WEEKDAY = 3;

// Charged up front, before the first round.
constexpr int GRUDGE_ENTRY_FEE = 30;

// The gate the garage applies alongside the day check.
constexpr int GRUDGE_MIN_GAS = 24;

// Four rounds at most.
constexpr int GRUDGE_MAX_ROUNDS = 4;

// race_type is 0x04 for the opening round and 0x08 for every round after it.
constexpr uint16_t GRUDGE_RACE_TYPE_A = 0x04;
constexpr uint16_t GRUDGE_RACE_TYPE_B = 0x08;

inline uint16_t GrudgeRaceTypeForRound(int round) {
    return round <= 0 ? GRUDGE_RACE_TYPE_A : GRUDGE_RACE_TYPE_B;
}

inline bool GrudgeNightOpen(int dayNumber) {
    // Guard the negative case rather than lean on C's remainder sign.
    if (dayNumber < 0) {
        return false;
    }
    return (dayNumber % GRUDGE_DAYS_PER_WEEK) == GRUDGE_WEEKDAY;
}

// Driver stats live in 1..8 - the range dp->skill and dp->reflex assert.
constexpr int GRUDGE_STAT_MIN = 1;
constexpr int GRUDGE_STAT_MAX = 8;

struct GrudgeStats {
    int playerSkill = 4;
    int playerReflex = 4;
    int opponentSkill = 4;
    int opponentReflex = 4;
};

// A round moves both drivers, and the winner's number goes UP.
struct GrudgeRoundResult {
    bool wonSkill = false;
    bool wonReflex = false;
    // Every driver in the field carries a breakout.
    int opponentBreakoutTenths = 0;
};


void GrudgeApplyRoundResult(GrudgeStats& stats, const GrudgeRoundResult& result);

// The breakout time, in tenths.
inline bool GrudgeBrokeOut(int timeTenths, int breakoutTenths) {
    return timeTenths < breakoutTenths;
}

// A qualifier can also be refused for being too SLOW.
constexpr int GRUDGE_BREAKOUT_MAX_TENTHS = 801;

inline bool GrudgeBreakoutTooWeak(int timeTenths) {
    return timeTenths >= GRUDGE_BREAKOUT_MAX_TENTHS;
}
inline bool GrudgeOpponentBrokeOut(int timeTenths, int opponentBreakoutTenths) {
    return opponentBreakoutTenths > 0 && GrudgeBrokeOut(timeTenths, opponentBreakoutTenths);
}


// What a placing pays.
constexpr int GRUDGE_PAYING_PLACES = 3;
constexpr int GRUDGE_PAYOUT[GRUDGE_PAYING_PLACES] = {150, 100, 50};

// placing is 0 based, so 0 is first. Pays 0 outside the top three.
inline int GrudgePayout(int placing) {
    if (placing < 0 || placing >= GRUDGE_PAYING_PLACES) {
        return 0;
    }
    return GRUDGE_PAYOUT[placing];
}

// The bracket handicap.
constexpr int GRUDGE_STAGE_FRAMES = 110;
constexpr int GRUDGE_HANDICAP_FRAMES_PER_TENTH = 7;

struct GrudgeStageDelay {
    int playerFrames = GRUDGE_STAGE_FRAMES;
    int opponentFrames = GRUDGE_STAGE_FRAMES;
};

// The slower dial launches first, by the difference between the two.
inline GrudgeStageDelay GrudgeStageDelays(int playerBreakoutTenths,
                                          int opponentBreakoutTenths) {
    GrudgeStageDelay out;
    const int delta = opponentBreakoutTenths - playerBreakoutTenths;
    const int penalty = (delta < 0 ? -delta : delta) * GRUDGE_HANDICAP_FRAMES_PER_TENTH;
    if (delta > 0) {
        out.playerFrames += penalty;      // the player's dial is quicker, so they wait
    } else {
        out.opponentFrames += penalty;
    }
    return out;
}

// The bracket, lgrudge.c FUN_00253bce.
constexpr int GRUDGE_FIELD_SIZE = 10;
constexpr int GRUDGE_CAR_CASES = 21;

// Record +2 is who is still in.
constexpr int GRUDGE_IN = 0;
constexpr int GRUDGE_OUT = -1;
constexpr int GRUDGE_OUT_LATE = 2;

// Record +0 is the driver.
constexpr int GRUDGE_PLAYER = -1;
constexpr int GRUDGE_EMPTY = -2;

struct GrudgeFieldEntry {
    int driver = GRUDGE_EMPTY;        // record +0
    int placing = GRUDGE_IN;          // record +2, the state above
    int breakoutTenths = 0;           // record +4
};

// The other pairs are not raced, they are decided by a draw: FUN_00253ec6 rolls
// FUN_0022fc6c (0..99) and compares against 70.
constexpr int GRUDGE_COINFLIP_THRESHOLD = 0x46;   // 70

// The predicted quarter for one car, before the /6 conversion.
int GrudgePredictedTime(int carCase, int driverByte);

// What goes in the record: the predicted time scaled by 10/6.
inline int GrudgeBreakoutForCar(int carCase, int driverByte) {
    return (GrudgePredictedTime(carCase, driverByte) * 10) / 6;
}

// Sort the field by breakout, quickest first, then pair it.
void GrudgeSeedBracket(GrudgeFieldEntry* field, int count);

// FUN_00253ec6: pairs are an even slot and the odd one after it.
int GrudgePlayerSlot(const GrudgeFieldEntry* field, int count);
int GrudgeOpponentSlot(const GrudgeFieldEntry* field, int count);

// Close out a round: the loser of the player's pair goes out and the other pairs are settled by the draw.
// Round 0 settles only the player's pair and one other (FUN_00253ec6), so ten racers go 10 -> 8 -> 4 -> 2 -> 1.
// The last round only orders the final two pairs: each loser drops one place.
void GrudgeAdvanceBracket(GrudgeFieldEntry* field, int count, int round,
                          bool playerWon, RaceRng& rng);

// Where the player came, 0 based, once the marks are final. Anything knocked
// out before the placings were handed out pays nothing, so it reads as 3.
int GrudgePlayerPlacing(const GrudgeFieldEntry* field, int count);

// Why a night ended.
enum class GrudgeOutcome : uint8_t {
    Racing,        // still in it
    Eliminated,    // lost early
    CrossedLine,   // message 54, crossing the centre line
    BrokeOut,      // message 55, quicker than the time you called
    WeakBreakout,  // message 56, the qualifier was too slow to stand
    Finished,      // came through all the rounds
};

// Losing the opening race - the one run before the loop - ends the night outright.
inline bool GrudgeLossEliminates(int round) {
    return round < 2;
}

// One night's progress. The opening race qualifies and fixes the breakout;
// up to four rounds follow.
struct GrudgeNightState {
    int round = -1;            // -1 before the qualifier, then 0..3
    int breakoutTenths = 0;
    int placing = 0;
    GrudgeOutcome outcome = GrudgeOutcome::Racing;
    GrudgeStats stats;
};

enum class GrudgeStep : uint8_t {
    Qualify,
    Race,
    Standings,
    Over,
};

inline bool GrudgeCanEnter(int dayNumber, int gas, int bankroll) {
    return GrudgeNightOpen(dayNumber) && gas >= GRUDGE_MIN_GAS &&
           bankroll >= GRUDGE_ENTRY_FEE;
}

// The opening race. Losing it ends the night, as does a time too slow to stand
// as a breakout; winning fixes the breakout at whatever time was run.
void GrudgeQualified(GrudgeNightState& night, bool won, int timeTenths);

// One bracket round.
bool GrudgeRoundDone(GrudgeNightState& night, const GrudgeRoundResult& result,
                     int timeTenths, bool crossedLine);

GrudgeStep GrudgeNextStep(const GrudgeNightState& night);

inline int GrudgeWinnings(const GrudgeNightState& night) {
    return night.outcome == GrudgeOutcome::Finished ? GrudgePayout(night.placing) : 0;
}

}  // namespace srrace
