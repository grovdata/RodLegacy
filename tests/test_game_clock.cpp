#include <doctest/doctest.h>
#include "core/game_clock.h"

TEST_CASE("GameClock: day-boundary dates match the confirmed epoch and month lengths") {
    CHECK(GameClock::FormatDate(0) == "June 16, 1963");                    // new-game epoch
    CHECK(GameClock::FormatDate(GameClock::TICKS_PER_DAY) == "June 17, 1963");
    CHECK(GameClock::FormatDate(2 * GameClock::TICKS_PER_DAY) == "June 18, 1963");
    CHECK(GameClock::FormatDate(14 * GameClock::TICKS_PER_DAY) == "June 30, 1963");   // June's last day
    CHECK(GameClock::FormatDate(15 * GameClock::TICKS_PER_DAY) == "July 1, 1963");
    CHECK(GameClock::FormatDate(45 * GameClock::TICKS_PER_DAY) == "July 31, 1963");   // July's last day
    CHECK(GameClock::FormatDate(46 * GameClock::TICKS_PER_DAY) == "August 1, 1963");
    CHECK(GameClock::FormatDate(76 * GameClock::TICKS_PER_DAY) == "August 31, 1963"); // August's last day
    CHECK(GameClock::FormatDate(77 * GameClock::TICKS_PER_DAY) == "September 1, 1963");
    CHECK(GameClock::FormatDate(90 * GameClock::TICKS_PER_DAY) == "September 14, 1963");
}

TEST_CASE("GameClock: season is capped at day 91 (September 15, 1963)") {
    CHECK(GameClock::DayNumber(91 * GameClock::TICKS_PER_DAY) == 91);
    CHECK(GameClock::FormatDate(91 * GameClock::TICKS_PER_DAY) == "September 15, 1963");
    // Far past the cap must still clamp, not roll over into a new season.
    CHECK(GameClock::DayNumber(1000000) == 91);
    CHECK(GameClock::FormatDate(1000000) == "September 15, 1963");
}

TEST_CASE("GameClock: in-day clock (hour = ticks/300 within the day, minute rescaled 0-59)") {
    CHECK(GameClock::FormatClock(0) == "00:00");
    CHECK(GameClock::FormatClock(299) == "00:59");
    CHECK(GameClock::FormatClock(300) == "01:00");
    CHECK(GameClock::FormatClock(GameClock::TICKS_PER_DAY - 1) == "11:59"); // last tick of the day
    CHECK(GameClock::FormatClock(GameClock::TICKS_PER_DAY) == "00:00");    // wraps into the next day
}

TEST_CASE("GameClock: hours<->ticks conversions round-trip at 150 ticks/hour") {
    CHECK(GameClock::TICKS_PER_HOUR == 150);
    CHECK(GameClock::TicksToHours(150) == 1);
    CHECK(GameClock::HoursToTicks(1) == 150);
    for (int hours : {0, 1, 5, 24, 100}) {
        CHECK(GameClock::TicksToHours(GameClock::HoursToTicks(hours)) == hours);
    }
}

// The anchors are the cyan rings the calendar art draws over the season boundaries, exactly 24x9.
TEST_CASE("GameClock: calendar highlight lands on the artwork's own season-boundary rings") {
    // June 15, 1963 is a Saturday: last column, third week row of the June
    // block. Ring bbox in lib.p 320 is x 143..158, y 28..36.
    CHECK(GameClock::CalendarHighlightPos(6, 15).x == 139);
    CHECK(GameClock::CalendarHighlightPos(6, 15).y == 28);

    // September 15 is a Sunday: first column, third week row of the September
    // block. Ring bbox in lib.p 319 is x 35..50, y 119..127.
    CHECK(GameClock::CalendarHighlightPos(9, 15).x == 31);
    CHECK(GameClock::CalendarHighlightPos(9, 15).y == 119);
}

TEST_CASE("GameClock: calendar highlight rows and columns follow the 1963 weekdays") {
    // Each month's 1st, at its real 1963 weekday column (0 = Sunday).
    CHECK(GameClock::CalendarHighlightPos(6, 1).x == 31 + 18 * 6);  // Saturday
    CHECK(GameClock::CalendarHighlightPos(7, 1).x == 31 + 18 * 1);  // Monday
    CHECK(GameClock::CalendarHighlightPos(8, 1).x == 31 + 18 * 4);  // Thursday
    CHECK(GameClock::CalendarHighlightPos(9, 1).x == 31 + 18 * 0);  // Sunday

    CHECK(GameClock::CalendarHighlightPos(6, 1).y == 14);
    CHECK(GameClock::CalendarHighlightPos(7, 1).y == 107);
    CHECK(GameClock::CalendarHighlightPos(8, 1).y == 15);
    CHECK(GameClock::CalendarHighlightPos(9, 1).y == 105);
    CHECK(GameClock::CalendarHighlightPos(9, 8).y == 105 + 7);   // one week on
    CHECK(GameClock::CalendarHighlightPos(9, 15).y == 105 + 14);

    CHECK(GameClock::CalendarHighlightPos(6, 16).x == 31);
    CHECK(GameClock::CalendarHighlightPos(6, 23).x == 31);
    CHECK(GameClock::CalendarHighlightPos(6, 30).x == 31);
    CHECK(GameClock::CalendarHighlightPos(7, 7).x == 31);
    CHECK(GameClock::CalendarHighlightPos(8, 4).x == 31);

    // Out of season, or a nonsense day, highlights nothing.
    CHECK(GameClock::CalendarHighlightPos(5, 1).x == 0);
    CHECK(GameClock::CalendarHighlightPos(5, 1).y == 0);
    CHECK(GameClock::CalendarHighlightPos(10, 1).y == 0);
    CHECK(GameClock::CalendarHighlightPos(6, 0).y == 0);
}
