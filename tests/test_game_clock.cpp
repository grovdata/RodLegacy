#include <doctest/doctest.h>
#include "game_clock.h"

// Values here are the confirmed findings from RodLegacyResearch/docs/formats/
// save_game.md's "Chunk 7 fully decoded" section: code-level confirmation
// from the Amiga binary (CarEventLoop's "% 6" tick increment,
// GetCappedElapsedDayNumber's literal 3600/91, GetMonthAndDayOfMonth's
// branch table), cross-checked live against a fresh "New Game" (June 16)
// and two further observed day transitions (June 17, June 18).

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
