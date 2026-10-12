#include "race/race_end_message.h"
#include "race/race_sim.h"
#include "core/game_text.h"

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

namespace {
RaceEndMessage M(GameText::Message id, MessageBox box) { return {GameText::Msg(id), box, id}; }
}

RaceEndMessage BuildRaceEndReason(const RaceEndContext& ctx) {
    // Before the other failures: the ticket is added on top of whatever else ended the race.
    // Unable to pay means jail and the end of the run.
    if (ctx.endFlags & END_COP_CAUGHT) {
        if (ctx.bankroll < ctx.speedingFine) {
            return M(GameText::JAILED_FOR_DEBTS, MessageBox::Wide);
        }
        return {GameText::Format(GameText::FINED, ctx.speedingFine), MessageBox::Wide, kFinedMessage};
    }
    switch (ClassifyCrash(ctx)) {
        case CrashOutcome::Forfeited:
            // The car goes with the bet, so no repair bill and no scrap money.
            if (ctx.tiresBlown) {
                return M(GameText::TIRES_BLOWN_PINK, MessageBox::TwoRow);
            }
            return M(GameText::WRECK_PINK, MessageBox::Wide);
        case CrashOutcome::Totaled:
            return {GameText::Msg(GameText::TOTALED, ScrapPayout(ctx.carStrippedValue)), MessageBox::TwoRow,
                    GameText::TOTALED};
        case CrashOutcome::DroveAway:
            if (ctx.tiresBlown) {
                return M(GameText::SLOW_CRASH_TIRES, MessageBox::TwoRow);
            }
            return M(GameText::SLOW_CRASH, MessageBox::TwoRow);
        case CrashOutcome::RepairOrJunk:
            // No bubble: straight to the repair quote dialog.
            return {};
        case CrashOutcome::None:
            break;
    }
    if (ctx.endFlags & END_ENGINE_BLOWN) {
        if (ctx.isPinkSlip) {
            return M(GameText::ENGINE_BLOWN_PINK, MessageBox::TwoRow);
        }
        return M(GameText::ENGINE_BLOWN, MessageBox::Wide);
    }
    if (ctx.endFlags & END_TRANS_DROPPED) {
        if (ctx.isPinkSlip) {
            return M(GameText::TRANS_DROPPED_PINK, MessageBox::TwoRow);
        }
        return M(GameText::TRANS_DROPPED, MessageBox::Wide);
    }
    if (ctx.endFlags & END_STALLED) {
        return M(GameText::JUMPED_LIGHT, MessageBox::Wide);
    }

    // A grudge round's result is the night's own message, shown on the Grudge Night screen.
    if (ctx.isGrudge) {
        return {};
    }

    if (ctx.playerWon) {
        if (ctx.vsKing) {
            // Street Rod 2 crowns you only on the Aqueduct, the last of his three (lresult.c 0x25b6b2).
            const bool final = ctx.isSr2 ? ctx.isAqueduct : !ctx.isDragRace;
            if (!final) return M(GameText::BEAT_KING_DRAG, MessageBox::Wide);
            return M(GameText::BEAT_KING_ROAD, MessageBox::TwoRow);
        }
        if (ctx.isPinkSlip) return M(GameText::WON_CAR, MessageBox::Wide);
        switch (ctx.betAmount) {
            case 10:  return M(GameText::WON_10, MessageBox::Wide);
            case 25:  return M(GameText::WON_25, MessageBox::Wide);
            case 50:  return M(GameText::WON_50, MessageBox::Wide);
            case 100: return M(GameText::WON_100, MessageBox::Wide);
            case 150: return M(GameText::WON_150, MessageBox::Wide);
            default:  break;
        }
        return M(GameText::WON_NO_BET, MessageBox::Wide);
    }

    if (ctx.playerLost) {
        if (ctx.vsKing) {
            if (ctx.isDragRace) return M(GameText::LOST_TO_KING_DRAG, MessageBox::Wide);
            return M(GameText::LOST_TO_KING_ROAD, MessageBox::TwoRow);
        }
        if (ctx.isPinkSlip) return M(GameText::LOST_PINK_SLIP, MessageBox::Narrow);
        return M(GameText::LOST, MessageBox::Narrow);
    }
    // No original string for this state.
    return {};
}

}
