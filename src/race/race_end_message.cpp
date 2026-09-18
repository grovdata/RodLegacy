#include "race/race_end_message.h"
#include "race/race_sim.h"

namespace srrace {

CrashOutcome ClassifyCrash(const RaceEndContext& ctx) {
    // The cop's branch is checked before the crash tree in ProcessRaceOutcome,
    // so being busted costs you the ticket and nothing else.
    if (ctx.endFlags & END_COP_CAUGHT) return CrashOutcome::None;
    if ((ctx.endFlags & END_WRECK_MASK) == 0) return CrashOutcome::None;
    if (ctx.isPinkSlip) return CrashOutcome::Forfeited;
    if (ctx.crashSpeed > 90) return CrashOutcome::Totaled;
    if (ctx.crashSpeed <= 30) return CrashOutcome::DroveAway;
    return CrashOutcome::RepairOrJunk;
}

bool SettlesBet(const RaceEndContext& ctx) {
    return (ctx.endFlags & END_COP_CAUGHT) == 0;
}

RaceEndMessage BuildRaceEndReason(const RaceEndContext& ctx) {
    // Pulled over for speeding. Checked before the other failures because the
    // ticket is added on top of whatever else ended the race (a crash while
    // being chased gets you both), and the cop is the more specific news.
    // If you cannot cover it you go to jail and the run is over - the original
    // shows the debts line and sets its game-over bit (ProcessRaceOutcome).
    if (ctx.endFlags & END_COP_CAUGHT) {
        if (ctx.bankroll < ctx.speedingFine) {
            return {"You've been sentenced for debts, Ace`!", MessageBox::Wide};  // 81
        }
        return {"You have been fined $" + std::to_string(ctx.speedingFine) +
                " for speeding.", MessageBox::Wide};
    }
    switch (ClassifyCrash(ctx)) {
        case CrashOutcome::Forfeited:
            // The car goes with the bet either way, so there is no repair bill
            // and no scrap money - just the short line.
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
            // No bubble at all: the original goes straight to the repair quote
            // dialog (Driving::ShowWreckDialog).
            return {};
        case CrashOutcome::None:
            break;
    }
    if (ctx.endFlags & END_ENGINE_BLOWN) {
        return {"Blew your engine again, huh, Ace`?", MessageBox::Wide};  // 87
    }
    if (ctx.endFlags & END_TRANS_DROPPED) {
        return {"You just dropped your`transmission,klutz`!", MessageBox::Wide};  // 86
    }
    if (ctx.endFlags & END_STALLED) {
        return {"Ya jumped the light, speedy`!", MessageBox::Wide};  // 80
    }

    if (ctx.playerWon) {
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
        // Confirmed by playtesting: a plain drag loss shows the short "You
        // lose, sucker." line. See RodLegacyResearch/docs/driving/race_end_messages.md.
        if (ctx.isDragRace) {
            return {"You lose, sucker.", MessageBox::Narrow};  // 83
        }
        if (ctx.tiresBlown) {
            return {"Lucky you're so slow. You only lost\nthe race. But you need new tires.",
                    MessageBox::TwoRow};  // 94
        }
        return {"Lucky you're so slow. You only lost\nthe race.", MessageBox::TwoRow};  // 93
    }
    // Unresolved: the Amiga strings "Your winning time %d.%d sec., avg.
    // speed %2d mph" / "Congratulations !" and "In %d.%d sec you covered only
    // %2d%% of the distance. Your avg. speed %2d mph." exist in the binary
    // (BuildResultText, called from both the win and the outcome==1 loss
    // branches of ProcessRaceOutcome) and look drag-race-shaped, but they sit
    // in a different string cluster than the bubble lines above (near
    // garage/gas-station UI text, not the diner message block) and
    // BuildResultText's win/lose gating condition doesn't cleanly map to
    // "drag vs road" from the decompile alone. Not implemented here rather
    // than guess wrong - see RodLegacyResearch/docs/driving/race_end_messages.md.

    // Nothing matched: no bubble at all, the same as the RepairOrJunk branch.
    // There is no original string for this state.
    return {};
}

} // namespace srrace
