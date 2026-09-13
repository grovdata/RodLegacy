#pragma once
#include <cstdint>
#include <string>

// Street Rod's in-game elapsed-time/calendar model, confirmed this session
// against the Amiga binary's own code (RodLegacyResearch/docs/formats/
// save_game.md -- "Chunk 7 fully decoded"). Deliberately free of any
// dependency on State so it's cheap to unit test: every function here is a
// pure function of a raw tick count.
//
//   ticks advance 1 per 6 VBlanks (CarEventLoop's own event loop, PAL 50Hz
//   -> ~8.333 ticks/second), independent of any player action.
//   day_number = min(ticks / 3600, 91); 0 = June 16, 1963 (new-game epoch).
//   Season is a fixed ~91-day window, capped at September 15, 1963.

namespace GameClock {

constexpr int TICKS_PER_DAY = 3600;
constexpr int TICKS_PER_HOUR = TICKS_PER_DAY / 24;  // 150
constexpr int SEASON_END_DAY = 91;                  // caps at September 15, 1963

struct GameDate {
    const char* month;
    int monthNumber;  // 6..9, for callers that index by month (the calendar)
    int dayOfMonth;
    int year = 1963;
};

struct GameTime {
    int hour;    // 0-11 (twelve 300-tick blocks/day)
    int minute;  // 0-59
};

// Where the 24x9 "current date" highlight goes on a calendar sheet, relative to
// that sheet's top-left. lib.p 320 holds June+July, 319 holds August+September;
// both are 168x150 with day columns Sunday..Saturday on an 18px pitch and week
// rows on a 7px pitch. Measured off the bitmaps: each sheet has a cyan ring
// drawn over a season boundary (June 15 / September 15) that is exactly 24x9
// and lands on the cell this returns.
struct CalendarPos {
    int x;
    int y;
};
CalendarPos CalendarHighlightPos(int month, int day);  // month 6..9; {0,0} if out of season

int DayNumber(uint32_t elapsedTicks);
GameDate GetDate(uint32_t elapsedTicks);
GameTime GetTime(uint32_t elapsedTicks);

std::string FormatDate(uint32_t elapsedTicks);   // "June 16, 1963"
std::string FormatClock(uint32_t elapsedTicks);  // "00:00"

// Hours-granularity convenience conversions, for callers (existing State
// call sites, CalculateScore) that work in hours rather than raw ticks.
int TicksToHours(uint32_t elapsedTicks);
uint32_t HoursToTicks(int hours);

}  // namespace GameClock
