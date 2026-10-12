/*
 * RodLegacy - The 1963 summer calendar: GameClock converts the game's tick counter to dates, times and
 * calendar positions.
 *
 * Created: 2026-08-04
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <cstdint>
#include <string>

// Ticks advance 1 per 6 PAL VBlanks (~8.333/s), independent of player action.
// day_number = min(ticks / 3600, 91); 0 = June 16, 1963.

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

// lib.p 320 holds June+July, 319 August+September; 18px day columns, 7px week rows.
struct CalendarPos {
    int x;
    int y;
};
CalendarPos CalendarHighlightPos(int month, int day);  // month 6..9; {0,0} if out of season

int DayNumber(uint32_t elapsedTicks);
GameDate GetDate(uint32_t elapsedTicks);
GameTime GetTime(uint32_t elapsedTicks);

// The year is only text: Street Rod 2 reuses the 1963 calendar art and prints 1969.
std::string FormatDate(uint32_t elapsedTicks, int year = 1963);   // "June 16, 1963"
std::string FormatClock(uint32_t elapsedTicks);  // "00:00"

int TicksToHours(uint32_t elapsedTicks);
uint32_t HoursToTicks(int hours);

}
