#include "game_clock.h"
#include <algorithm>
#include <cstdio>

namespace GameClock {

int DayNumber(uint32_t elapsedTicks) {
    return std::min<int>(static_cast<int>(elapsedTicks / TICKS_PER_DAY), SEASON_END_DAY);
}

GameDate GetDate(uint32_t elapsedTicks) {
    int dayNumber = DayNumber(elapsedTicks);
    GameDate date;
    if (dayNumber < 15)      { date.month = "June";      date.dayOfMonth = dayNumber + 16; }
    else if (dayNumber < 46) { date.month = "July";      date.dayOfMonth = dayNumber - 14; }
    else if (dayNumber < 77) { date.month = "August";    date.dayOfMonth = dayNumber - 45; }
    else if (dayNumber < 91) { date.month = "September"; date.dayOfMonth = dayNumber - 76; }
    else                     { date.month = "September"; date.dayOfMonth = 15; }
    return date;
}

GameTime GetTime(uint32_t elapsedTicks) {
    int rem = static_cast<int>(elapsedTicks % TICKS_PER_DAY);
    GameTime time;
    time.hour = rem / 300;
    time.minute = (rem % 300) * 60 / 300;
    return time;
}

std::string FormatDate(uint32_t elapsedTicks) {
    GameDate date = GetDate(elapsedTicks);
    return std::string(date.month) + " " + std::to_string(date.dayOfMonth) + ", " + std::to_string(date.year);
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

}  // namespace GameClock
