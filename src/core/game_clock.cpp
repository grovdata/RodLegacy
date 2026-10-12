#include "core/game_clock.h"
#include <algorithm>
#include <cstdio>

namespace GameClock {

namespace {
constexpr int kCol0CenterX = 43;
constexpr int kColPitch    = 18;
constexpr int kRowPitch    = 7;
// The August sheet sits one pixel lower than June's.
constexpr int kRow0Y[4]    = {18, 111, 19, 109};  // June, July, August, September
// Weekday of the 1st in 1963, 0 = Sunday.
constexpr int kFirstDow[4] = {6, 1, 4, 0};        // Jun Sat, Jul Mon, Aug Thu, Sep Sun
constexpr int kHighlightW  = 24;
constexpr int kHighlightH  = 9;
}

CalendarPos CalendarHighlightPos(int month, int day) {
    if (month < 6 || month > 9 || day < 1 || day > 31) {
        return {0, 0};
    }
    const int m = month - 6;
    const int cell = day - 1 + kFirstDow[m];
    return {kCol0CenterX + kColPitch * (cell % 7) - kHighlightW / 2,
            kRow0Y[m] + kRowPitch * (cell / 7) - kHighlightH / 2};
}

int DayNumber(uint32_t elapsedTicks) {
    return std::min<int>(static_cast<int>(elapsedTicks / TICKS_PER_DAY), SEASON_END_DAY);
}

GameDate GetDate(uint32_t elapsedTicks) {
    int dayNumber = DayNumber(elapsedTicks);
    GameDate date;
    if (dayNumber < 15)      { date.month = "June";      date.monthNumber = 6; date.dayOfMonth = dayNumber + 16; }
    else if (dayNumber < 46) { date.month = "July";      date.monthNumber = 7; date.dayOfMonth = dayNumber - 14; }
    else if (dayNumber < 77) { date.month = "August";    date.monthNumber = 8; date.dayOfMonth = dayNumber - 45; }
    else if (dayNumber < 91) { date.month = "September"; date.monthNumber = 9; date.dayOfMonth = dayNumber - 76; }
    else                     { date.month = "September"; date.monthNumber = 9; date.dayOfMonth = 15; }
    return date;
}

GameTime GetTime(uint32_t elapsedTicks) {
    int rem = static_cast<int>(elapsedTicks % TICKS_PER_DAY);
    GameTime time;
    time.hour = rem / 300;
    time.minute = (rem % 300) * 60 / 300;
    return time;
}

std::string FormatDate(uint32_t elapsedTicks, int year) {
    GameDate date = GetDate(elapsedTicks);
    return std::string(date.month) + " " + std::to_string(date.dayOfMonth) + ", " + std::to_string(year);
}

std::string FormatClock(uint32_t elapsedTicks) {
    GameTime time = GetTime(elapsedTicks);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", time.hour, time.minute);
    return std::string(buf);
}

int TicksToHours(uint32_t elapsedTicks) {
    return static_cast<int>(elapsedTicks / TICKS_PER_HOUR);
}

uint32_t HoursToTicks(int hours) {
    return static_cast<uint32_t>(hours) * TICKS_PER_HOUR;
}

}
