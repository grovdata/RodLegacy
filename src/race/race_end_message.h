#pragma once
// Race-end bubble text, split out of Racing::GetRaceEndReason() so the pure
// decision logic can be unit tested without SDL (Racing owns a SDL_Surface
// and an AudioManager*, neither buildable in the SDL-free test target).
// The strings are the originals, taken verbatim from the message table in the
// Amiga executable (Street_Rod ~0x246e00-0x2479ff); see
// RodLegacyResearch/docs/driving/race_end_messages.md for the string-table
// research, and race_sim.h for the END_* flag bits used here.

#include <cstdint>
#include <string>

namespace srrace {

// Everything GetRaceEndReason() needs to pick a line, gathered by the caller
// (Racing::GetRaceEndReason()) from CarState::endFlags, RaceOutcome, and the
// race setup snapshotted at InitializeRaceSimulation().
struct RaceEndContext {
    uint16_t endFlags = 0;   // sim.Player().endFlags
    bool playerWon = false;  // raceOutcome == RaceOutcome::PlayerWon
    bool playerLost = false; // raceOutcome == RaceOutcome::PlayerLost
    bool isPinkSlip = false;
    int betAmount = 0;
    bool isDragRace = false;      // currentTrackType == SR_TRACK_DRAG_RACE
    bool tireWearIncreased = false; // sim.Player().tireWearDelta > 0
    int speedingFine = 0;         // sim.SpeedingFine()
    int bankroll = 0;             // state.GetBankroll()
    int crashSpeed = 0;           // sim.Player().speed, frozen at the impact
    int carValue = 0;             // state.GetCarValue(current car)
    int carStrippedValue = 0;     // state.GetCarStrippedValue(current car)
};

std::string BuildRaceEndReason(const RaceEndContext& ctx);

// What a wreck costs you. ProcessRaceOutcome (0x23eaa2) tiers this on the
// speed at the moment of impact (A4+0x4b0a), the same 31 / 61 / 91 mph
// thresholds the crash graphics use.
enum class CrashOutcome {
    None,          // not a wreck, or the cop got you first (his branch wins)
    Forfeited,     // pink-slip race: the car goes with the bet regardless
    DroveAway,     // <= 30 mph, no bill
    RepairOrJunk,  // 31..90 mph, the "Fix it" / "Junk it" dialog
    Totaled,       // 91+ mph, the car is scrap and gone
};

CrashOutcome ClassifyCrash(const RaceEndContext& ctx);

// Whether the stake changes hands at all. ProcessRaceOutcome's cop branch shows
// the fine and returns before the bet, the reputation and the racer pool are
// touched, so a ticket costs you the fine and nothing else - being pulled over
// in a pink-slip race does not cost you the car. See docs/driving/cop.md.
bool SettlesBet(const RaceEndContext& ctx);

// Wreck payouts, straight off the original's arithmetic. Scrapping strips the
// car first, so it is paid on DismantleCar's return; junking it after a repair
// quote is paid on the car's full value, parts and all.
inline int ScrapPayout(int strippedValue) { return strippedValue / 10 + 19; }
inline int JunkPayout(int carValue) { return carValue * 4 / 10; }
// The repair quote is $100 + rand(N) - N is 20 up to 60 mph, 40 above.
inline int RepairRollRange(int crashSpeed) { return crashSpeed > 60 ? 40 : 20; }

} // namespace srrace
