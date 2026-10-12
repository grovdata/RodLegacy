#include "race/grudge.h"

#include <algorithm>

namespace srrace {

namespace {

int StepToward(int value, int by) {
    const int moved = value + by;
    if (moved < GRUDGE_STAT_MIN) {
        return GRUDGE_STAT_MIN;
    }
    if (moved > GRUDGE_STAT_MAX) {
        return GRUDGE_STAT_MAX;
    }
    return moved;
}

}  // namespace

// FUN_00253bce's switch. `base` is -driverByte and `b10` is 10 - driverByte,
// exactly as the original forms them.
int GrudgePredictedTime(int carCase, int driverByte) {
    const int base = -driverByte;
    const int b10 = 10 - driverByte;
    switch (carCase) {
        case 0:
        case 1:
        case 7:
        case 9:
        case 0x12: return 158;
        case 2:    return (b10 < 6 ? 0 : (base + 5) * 4) + b10 * 3 + 121;
        case 3:    return b10 * 4 + 103;
        case 4:    return b10 * 3 + 120;
        case 5:    return base + 99;
        case 6:    return base + 87;
        case 8:    return b10 * 2 + 106;
        case 10:   return 122;
        case 0xb:  return b10 * 2 + 106;
        case 0xc:  return base + 84;
        case 0xd:  return (b10 < 6 ? 0 : 6) + 88;
        case 0xe:  return b10 * 2 + 101;
        case 0xf:  return b10 * 4 + 115;
        case 0x10: return base + 96;
        case 0x11: return b10 * 2 + 88;
        case 0x13: return b10 * 5 + 111;
        case 0x14: return 90;
        default:   return 71;
    }
}

void GrudgeSeedBracket(GrudgeFieldEntry* field, int count) {
    if (field == nullptr || count <= 1) {
        return;
    }
    // Selection sort, quickest breakout first.
    for (int i = 0; i < count; ++i) {
        int best = 32000;
        int bestIdx = i;
        for (int j = i + 1; j < count; ++j) {
            if (field[j].breakoutTenths < best) {
                best = field[j].breakoutTenths;
                bestIdx = j;
            }
        }
        if (best < field[i].breakoutTenths) {
            std::swap(field[i], field[bestIdx]);
        }
    }

    // Then four right rotations of shrinking windows, which is what turns the
    // sorted order into the pairings.
    int span = 4;
    for (int i = 1; i + 1 < count && span > 0; i += 2) {
        int src = span + i;
        if (src >= count) {
            break;
        }
        const GrudgeFieldEntry saved = field[src];
        while (i < src) {
            field[src] = field[src - 1];
            --src;
        }
        field[i] = saved;
        --span;
    }
}

int GrudgePlayerSlot(const GrudgeFieldEntry* field, int count) {
    if (field == nullptr) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        if (field[i].driver == GRUDGE_PLAYER) {
            return i;
        }
    }
    return -1;
}

int GrudgeOpponentSlot(const GrudgeFieldEntry* field, int count) {
    const int me = GrudgePlayerSlot(field, count);
    if (me < 0 || field[me].placing < 0) {
        return -1;
    }
    // Pairs are the drivers on the same mark, taken two at a time in slot order.
    const int group = field[me].placing;
    int prev = -1;
    for (int i = 0; i < count; ++i) {
        if (field[i].placing != group || field[i].driver == GRUDGE_EMPTY) {
            continue;
        }
        if (prev < 0) {
            prev = i;
            continue;
        }
        if (prev == me) {
            return i;
        }
        if (i == me) {
            return prev;
        }
        prev = -1;
    }
    return -1;
}

void GrudgeAdvanceBracket(GrudgeFieldEntry* field, int count, int round,
                          bool playerWon, RaceRng& rng) {
    if (field == nullptr || count <= 1) {
        return;
    }
    // Knocked out late, so second and third can be told from the rest, once the rounds that decide a placing
    // are reached.
    const bool bumps = round >= GRUDGE_MAX_ROUNDS - 1;
    const int loserMark = (round >= 2) ? GRUDGE_OUT_LATE : GRUDGE_OUT;
    auto lost = [&](int slot) {
        if (bumps) {
            field[slot].placing += 1;
        } else {
            field[slot].placing = loserMark;
        }
    };

    const int me = GrudgePlayerSlot(field, count);
    const int them = GrudgeOpponentSlot(field, count);
    if (me >= 0 && them >= 0) {
        lost(playerWon ? them : me);
    }

    auto settle = [&](int a, int b) {
        const int roll = static_cast<int>(rng.Next(100));
        lost(roll < GRUDGE_COINFLIP_THRESHOLD ? b : a);
    };

    if (round <= 0) {
        // One other pair only, from the top down, skipping the player's. Ten
        // racers less two leaves eight, which halves cleanly for the rest.
        for (int hi = count - 1; hi >= 1; hi -= 2) {
            if (hi == me || hi == them || hi - 1 == me || hi - 1 == them ||
                field[hi].driver == GRUDGE_EMPTY ||
                field[hi - 1].driver == GRUDGE_EMPTY) {
                continue;
            }
            settle(hi - 1, hi);
            break;
        }
        return;
    }

    // Every open pair, taken from the survivors in slot order the same way the opponent is picked.
    int prev = -1;
    for (int i = 0; i < count; ++i) {
        if (field[i].placing < 0 || field[i].driver == GRUDGE_EMPTY ||
            i == me || i == them) {
            continue;
        }
        if (prev < 0) {
            prev = i;
            continue;
        }
        if (field[prev].placing != field[i].placing) {
            prev = i;
            continue;
        }
        settle(prev, i);
        prev = -1;
    }
}

int GrudgePlayerPlacing(const GrudgeFieldEntry* field, int count) {
    const int me = GrudgePlayerSlot(field, count);
    if (me < 0 || field[me].placing < 0) {
        return GRUDGE_PAYING_PLACES;
    }
    return field[me].placing;
}

void GrudgeApplyRoundResult(GrudgeStats& stats, const GrudgeRoundResult& result) {
    // The winner's number rises and the loser's falls, and the two metrics are
    // judged independently.
    stats.playerReflex = StepToward(stats.playerReflex, result.wonReflex ? +1 : -1);
    stats.opponentReflex = StepToward(stats.opponentReflex, result.wonReflex ? -1 : +1);

    stats.playerSkill = StepToward(stats.playerSkill, result.wonSkill ? +1 : -1);
    stats.opponentSkill = StepToward(stats.opponentSkill, result.wonSkill ? -1 : +1);
}

void GrudgeQualified(GrudgeNightState& night, bool won, int timeTenths) {
    if (!won) {
        night.outcome = GrudgeOutcome::Eliminated;
        return;
    }
    // The original checks the time before it prints the breakout, so a slow
    // enough run ends the night even though the race was won.
    if (GrudgeBreakoutTooWeak(timeTenths)) {
        night.outcome = GrudgeOutcome::WeakBreakout;
        return;
    }
    night.breakoutTenths = timeTenths;
    night.round = 0;
}

bool GrudgeRoundDone(GrudgeNightState& night, const GrudgeRoundResult& result,
                     int timeTenths, bool crossedLine) {
    if (night.outcome != GrudgeOutcome::Racing) {
        return false;
    }
    if (crossedLine) {
        night.outcome = GrudgeOutcome::CrossedLine;
        return false;
    }
    if (GrudgeBrokeOut(timeTenths, night.breakoutTenths)) {
        night.outcome = GrudgeOutcome::BrokeOut;
        return false;
    }

    // Losing on the road but taking the round because the other driver broke out.
    GrudgeRoundResult judged = result;
    if (!judged.wonSkill &&
        GrudgeOpponentBrokeOut(timeTenths, result.opponentBreakoutTenths)) {
        judged.wonSkill = true;
        judged.wonReflex = true;
    }

    GrudgeApplyRoundResult(night.stats, judged);

    if (!judged.wonSkill && GrudgeLossEliminates(night.round)) {
        night.outcome = GrudgeOutcome::Eliminated;
        return false;
    }

    ++night.round;
    if (night.round >= GRUDGE_MAX_ROUNDS) {
        night.outcome = GrudgeOutcome::Finished;
    }
    return judged.wonSkill;
}

GrudgeStep GrudgeNextStep(const GrudgeNightState& night) {
    if (night.outcome == GrudgeOutcome::Finished) {
        return GrudgeStep::Standings;
    }
    if (night.outcome != GrudgeOutcome::Racing) {
        return GrudgeStep::Over;
    }
    return night.round < 0 ? GrudgeStep::Qualify : GrudgeStep::Race;
}

}  // namespace srrace
