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
};

std::string BuildRaceEndReason(const RaceEndContext& ctx);

} // namespace srrace
