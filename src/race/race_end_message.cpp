#include "race/race_end_message.h"
#include "race/race_sim.h"

namespace srrace {

CrashOutcome ClassifyCrash(const RaceEndContext& ctx) {
    // The cop's branch comes before the crash tree, so being busted costs only the ticket.
    if (ctx.endFlags & END_COP_CAUGHT) return CrashOutcome::None;
    if ((ctx.endFlags & END_WRECK_MASK) == 0) return CrashOutcome::None;
    if (ctx.isPinkSlip) return CrashOutcome::Forfeited;
    if (ctx.crashSpeed > 90) return CrashOutcome::Totaled;
    if (ctx.crashSpeed <= 30) return CrashOutcome::DroveAway;
    return CrashOutcome::RepairOrJunk;
}

bool SettlesBet(const RaceEndContext& ctx) {
    return (ctx.endFlags & (END_COP_CAUGHT | END_STALLED)) == 0;
}

RaceEndMessage BuildRaceEndReason(const RaceEndContext& ctx) {
    // Before the other failures: the ticket is added on top of whatever else ended the race.
    // Unable to pay means jail and the end of the run.
    if (ctx.endFlags & END_COP_CAUGHT) {
        if (ctx.bankroll < ctx.speedingFine) {
            return {"You've been sentenced for debts, Ace`!", MessageBox::Wide};  // 81
        }
        return {"You have been fined $" + std::to_string(ctx.speedingFine) +
                " for speeding.", MessageBox::Wide};
    }
    switch (ClassifyCrash(ctx)) {
        case CrashOutcome::Forfeited:
            // The car goes with the bet, so no repair bill and no scrap money.
            if (ctx.tiresBlown) {
                return {"Blew your tires again`?\nYou lose. Take a walk, sucker.",
                        MessageBox::TwoRow};  // 91
            }
            return {"You lose. Take a walk, sucker.", MessageBox::Wide};  // 90
        case CrashOutcome::Totaled:
            return {"Lucky you weren't killed, 'cause the car\nis totaled. You get $" +
                    std::to_string(ScrapPayout(ctx.carStrippedValue)) + " for scrap.",
                    MessageBox::TwoRow};  // 92
        case CrashOutcome::DroveAway:
            if (ctx.tiresBlown) {
                return {"Lucky you're so slow. You only lost\nthe race. But you need new tires.",
                        MessageBox::TwoRow};  // 94
            }
            return {"Lucky you're so slow. You only lost\nthe race.", MessageBox::TwoRow};  // 93
        case CrashOutcome::RepairOrJunk:
            // No bubble: straight to the repair quote dialog.
            return {};
        case CrashOutcome::None:
            break;
    }
    if (ctx.endFlags & END_ENGINE_BLOWN) {
        if (ctx.isPinkSlip) {
            return {"Blew your engine again`?\nYou lose. Take a walk, sucker.",
                    MessageBox::TwoRow};  // 89
        }
        return {"Blew your engine again, huh, Ace`?", MessageBox::Wide};  // 87
    }
    if (ctx.endFlags & END_TRANS_DROPPED) {
        if (ctx.isPinkSlip) {
            return {"You just dropped your`transmission`!\nYou lose. Take a walk, sucker.",
                    MessageBox::TwoRow};  // 88
        }
        return {"You just dropped your`transmission,klutz`!", MessageBox::Wide};  // 86
    }
    if (ctx.endFlags & END_STALLED) {
        return {"Ya jumped the light, speedy`!", MessageBox::Wide};  // 80
    }

    if (ctx.playerWon) {
        if (ctx.vsKing) {
            if (ctx.isDragRace) return {"Beginner's luck, punk`!", MessageBox::Wide};  // 78
            return {"You ran a good race.\nYou deserve to be King of the County`!",
                    MessageBox::TwoRow};  // 79
        }
        if (ctx.isPinkSlip) return {"You just won a helluva car.", MessageBox::Wide};  // 77
        switch (ctx.betAmount) {
            case 10:  return {"Big deal. Ten lousy bucks.", MessageBox::Wide};      // 73
            case 25:  return {"Big deal. Twenty-five bucks.", MessageBox::Wide};    // 74
            case 50:  return {"Fifty smackers. Not bad.", MessageBox::Wide};        // 75
            case 100: return {"A hundred smackers, hot shot.", MessageBox::Wide};   // 76
            default:  break;
        }
        return {"You won! Too bad you didn't bet, though.", MessageBox::Wide};  // 72
    }

    if (ctx.playerLost) {
        if (ctx.vsKing) {
            if (ctx.isDragRace) return {"Close, but no cigar punk`!", MessageBox::Wide};  // 84
            return {"I`knew`you'd`choke`when`the`pressure`got`to`ya.\n"
                    "Better luck next time, sucker`!", MessageBox::TwoRow};  // 85
        }
        if (ctx.isPinkSlip) return {"Take a walk, sucker.", MessageBox::Narrow};  // 82
        return {"You lose, sucker.", MessageBox::Narrow};  // 83
    }
    // No original string for this state.
    return {};
}

}
