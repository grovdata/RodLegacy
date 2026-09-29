#pragma once
// Race-end bubble text, kept SDL-free for unit tests. The strings are the originals verbatim.

#include <cstdint>
#include <string>

namespace srrace {

struct RaceEndContext {
    uint16_t endFlags = 0;   // sim.Player().endFlags
    bool playerWon = false;  // raceOutcome == RaceOutcome::PlayerWon
    bool playerLost = false; // raceOutcome == RaceOutcome::PlayerLost
    bool isPinkSlip = false;
    int betAmount = 0;
    bool isDragRace = false;      // currentTrackType == SR_TRACK_DRAG_RACE
    bool tiresBlown = false; // endFlags & END_TIRES_BLOWN
    int speedingFine = 0;         // sim.SpeedingFine()
    int bankroll = 0;             // state.GetBankroll()
    int crashSpeed = 0;           // sim.Player().speed, frozen at the impact
    int carValue = 0;             // state.GetCarValue(current car)
    int carStrippedValue = 0;     // state.GetCarStrippedValue(current car)
};

enum class MessageBox {
    Narrow,  // box 0: 152x37, tail down-right
    Wide,    // box 1: 232x37, tail down-left
    TwoRow,  // box 2: 232x46, two rows
};

struct RaceEndMessage {
    std::string text;                    // empty = no bubble at all
    MessageBox box = MessageBox::Wide;
};

RaceEndMessage BuildRaceEndReason(const RaceEndContext& ctx);

// Tiered on the speed at impact, the same 31 / 61 / 91 mph thresholds the crash graphics use.
enum class CrashOutcome {
    None,          // not a wreck, or the cop got you first (his branch wins)
    Forfeited,     // pink-slip race: the car goes with the bet regardless
    DroveAway,     // <= 30 mph, no bill
    RepairOrJunk,  // 31..90 mph, the "Fix it" / "Junk it" dialog
    Totaled,       // 91+ mph, the car is scrap and gone
};

CrashOutcome ClassifyCrash(const RaceEndContext& ctx);

// A ticket costs the fine and nothing else: the original returns before the bet is settled.
bool SettlesBet(const RaceEndContext& ctx);

// Scrapping strips the car first, so it pays on the stripped value; junking after a repair quote pays full value.
inline int ScrapPayout(int strippedValue) { return strippedValue / 10 + 19; }
inline int JunkPayout(int carValue) { return carValue * 4 / 10; }
// The repair quote is $100 + rand(N) - N is 20 up to 60 mph, 40 above.
inline int RepairRollRange(int crashSpeed) { return crashSpeed > 60 ? 40 : 20; }

}
