#include "race/race_end_message.h"
#include "race/race_sim.h"

namespace srrace {

std::string BuildRaceEndReason(const RaceEndContext& ctx) {
    // Pulled over for speeding. Checked before the other failures because the
    // ticket is added on top of whatever else ended the race (a crash while
    // being chased gets you both), and the cop is the more specific news.
    // If you cannot cover it you go to jail and the run is over - the original
    // shows the debts line and sets its game-over bit (ProcessRaceOutcome).
    if (ctx.endFlags & END_COP_CAUGHT) {
        if (ctx.bankroll < ctx.speedingFine) {
            return "You've been sentenced for debts, Ace`!";
        }
        return "You have been fined $" + std::to_string(ctx.speedingFine) + " for speeding.";
    }
    if (ctx.endFlags & END_HIT_NPC)       return "You lose. Take a walk, sucker.";
    if (ctx.endFlags & END_WALL_HIT)      return "You lose. Take a walk, sucker.";
    if (ctx.endFlags & END_ENGINE_BLOWN)  return "Blew your engine again, huh, Ace`?";
    if (ctx.endFlags & END_TRANS_DROPPED) return "You just dropped your`transmission,klutz`!";
    if (ctx.endFlags & END_STALLED)       return "Ya jumped the light, speedy`!";

    if (ctx.playerWon) {
        if (ctx.isPinkSlip) return "You just won a helluva car.";
        switch (ctx.betAmount) {
            case 10:  return "Big deal. Ten lousy bucks.";
            case 25:  return "Big deal. Twenty-five bucks.";
            case 50:  return "Fifty smackers. Not bad.";
            case 100: return "A hundred smackers, hot shot.";
            default:  break;
        }
        return "You won! Too bad you didn't bet, though.";
    }

    if (ctx.playerLost) {
        // Confirmed by playtesting: a plain drag loss shows the short "You
        // lose, sucker." line. See RodLegacyResearch/docs/driving/race_end_messages.md.
        if (ctx.isDragRace) {
            return "You lose, sucker.";
        }
        // The original splits this over two lines ("...You only lost" / "the
        // race."); the bubble is single-line, so join with the ` the font
        // renders as a space, the same way the failure lines above do.
        if (ctx.tireWearIncreased) {
            return "Lucky you're so slow. You only lost`the race. But you need new tires.";
        }
        return "Lucky you're so slow. You only lost`the race.";
    }
    // Endings TODO:
    //  "Lucky you weren't killed, 'cause the car`is totaled. You get $N for scrap."
    // and the totaled-car follow-up, which is a dialog rather than a bubble:
    //  "Oh, well, it could have been worse.\n $N for parts and you are back on
    //  the streets." with a "Junk it" (and possibly "Fix it") button.
    //
    // Also unresolved: the Amiga strings "Your winning time %d.%d sec., avg.
    // speed %2d mph" / "Congratulations !" and "In %d.%d sec you covered only
    // %2d%% of the distance. Your avg. speed %2d mph." exist in the binary
    // (BuildResultText, called from both the win and the outcome==1 loss
    // branches of ProcessRaceOutcome) and look drag-race-shaped, but they sit
    // in a different string cluster than the bubble lines above (near
    // garage/gas-station UI text, not the diner message block) and
    // BuildResultText's win/lose gating condition doesn't cleanly map to
    // "drag vs road" from the decompile alone. Not implemented here rather
    // than guess wrong - see RodLegacyResearch/docs/driving/race_end_messages.md.

    return "RACE OVER";
}

} // namespace srrace
