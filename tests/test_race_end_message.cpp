#include <doctest/doctest.h>
#include "race/race_end_message.h"
#include "race/race_sim.h"

using srrace::RaceEndContext;
using srrace::BuildRaceEndReason;

TEST_SUITE("BuildRaceEndReason - failures (checked before win/loss)") {

TEST_CASE("cop caught, can afford the fine") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT;
    ctx.speedingFine = 50;
    ctx.bankroll = 100;
    CHECK(BuildRaceEndReason(ctx) == "You have been fined $50 for speeding.");
}

TEST_CASE("cop caught, cannot afford the fine") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT;
    ctx.speedingFine = 50;
    ctx.bankroll = 49;
    CHECK(BuildRaceEndReason(ctx) == "You've been sentenced for debts, Ace`!");
}

TEST_CASE("cop caught takes priority over a simultaneous playerLost/win flag") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT;
    ctx.speedingFine = 0;
    ctx.bankroll = 0;
    ctx.playerLost = true;
    CHECK(BuildRaceEndReason(ctx) == "You have been fined $0 for speeding.");
}

TEST_CASE("hit the NPC") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_HIT_NPC;
    ctx.crashSpeed = 20;
    CHECK(BuildRaceEndReason(ctx) == "Lucky you're so slow. You only lost`the race.");
}

TEST_CASE("hit the wall") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.crashSpeed = 20;
    CHECK(BuildRaceEndReason(ctx) == "Lucky you're so slow. You only lost`the race.");
}

TEST_CASE("engine blown") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_ENGINE_BLOWN;
    CHECK(BuildRaceEndReason(ctx) == "Blew your engine again, huh, Ace`?");
}

TEST_CASE("transmission dropped") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_TRANS_DROPPED;
    CHECK(BuildRaceEndReason(ctx) == "You just dropped your`transmission,klutz`!");
}

TEST_CASE("stalled / jumped the light") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_STALLED;
    CHECK(BuildRaceEndReason(ctx) == "Ya jumped the light, speedy`!");
}

} // TEST_SUITE

TEST_SUITE("BuildRaceEndReason - wins") {

TEST_CASE("pink slip win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.isPinkSlip = true;
    ctx.betAmount = 0;  // pink slip races carry no cash stake
    CHECK(BuildRaceEndReason(ctx) == "You just won a helluva car.");
}

TEST_CASE("$10 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 10;
    CHECK(BuildRaceEndReason(ctx) == "Big deal. Ten lousy bucks.");
}

TEST_CASE("$25 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 25;
    CHECK(BuildRaceEndReason(ctx) == "Big deal. Twenty-five bucks.");
}

TEST_CASE("$50 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 50;
    CHECK(BuildRaceEndReason(ctx) == "Fifty smackers. Not bad.");
}

TEST_CASE("$100 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 100;
    CHECK(BuildRaceEndReason(ctx) == "A hundred smackers, hot shot.");
}

TEST_CASE("win with no bet (just for kicks)") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 0;
    CHECK(BuildRaceEndReason(ctx) == "You won! Too bad you didn't bet, though.");
}

TEST_CASE("win with an unlisted bet amount falls back to the no-bet line") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 999;
    CHECK(BuildRaceEndReason(ctx) == "You won! Too bad you didn't bet, though.");
}

} // TEST_SUITE

TEST_SUITE("BuildRaceEndReason - losses") {

TEST_CASE("road race, plain loss") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isDragRace = false;
    CHECK(BuildRaceEndReason(ctx) == "Lucky you're so slow. You only lost`the race.");
}

TEST_CASE("road race, loss with tire wear") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isDragRace = false;
    ctx.tireWearIncreased = true;
    CHECK(BuildRaceEndReason(ctx) ==
          "Lucky you're so slow. You only lost`the race. But you need new tires.");
}

// Drag races get their own plain-loss line, confirmed by playtesting.
TEST_CASE("drag race, plain loss - drag-specific line, not the road one") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isDragRace = true;
    ctx.betAmount = 25;
    CHECK(BuildRaceEndReason(ctx) == "You lose, sucker.");
}

TEST_CASE("drag race loss ignores tire wear (that variant is road-only)") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isDragRace = true;
    ctx.tireWearIncreased = true;
    CHECK(BuildRaceEndReason(ctx) == "You lose, sucker.");
}

} // TEST_SUITE

// The crash tree of ProcessRaceOutcome (0x23eaa2): the speed at the impact
// decides whether you drive away, get a repair quote, or lose the car.
TEST_SUITE("ClassifyCrash - wreck tiers") {

using srrace::ClassifyCrash;
using srrace::CrashOutcome;

TEST_CASE("no wreck flag") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_ENGINE_BLOWN;
    ctx.crashSpeed = 120;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::None);
}

TEST_CASE("the cop's branch wins over the crash tree") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT | srrace::END_COP_CAUGHT;
    ctx.crashSpeed = 100;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::None);
}

TEST_CASE("pink slip: the car is forfeited whatever the speed") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.isPinkSlip = true;
    ctx.crashSpeed = 120;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::Forfeited);
    CHECK(BuildRaceEndReason(ctx) == "You lose. Take a walk, sucker.");
}

TEST_CASE("tier boundaries: 30 drives away, 31 gets a quote, 90 too, 91 totals it") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.crashSpeed = 30;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::DroveAway);
    ctx.crashSpeed = 31;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::RepairOrJunk);
    ctx.crashSpeed = 90;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::RepairOrJunk);
    ctx.crashSpeed = 91;
    CHECK(ClassifyCrash(ctx) == CrashOutcome::Totaled);
}

TEST_CASE("totaled prints the scrap payout, value/10 + 19") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_HIT_NPC;
    ctx.crashSpeed = 100;
    ctx.carStrippedValue = 1000;
    CHECK(srrace::ScrapPayout(ctx.carStrippedValue) == 119);
    CHECK(BuildRaceEndReason(ctx) ==
          "Lucky you weren't killed, 'cause the car`is totaled. You get $119 for scrap.");
}

TEST_CASE("the repair-quote tier has no bubble of its own") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.crashSpeed = 50;
    CHECK(BuildRaceEndReason(ctx).empty());
}

TEST_CASE("payout and quote arithmetic") {
    CHECK(srrace::JunkPayout(1000) == 400);
    CHECK(srrace::RepairRollRange(60) == 20);
    CHECK(srrace::RepairRollRange(61) == 40);
}

} // TEST_SUITE

TEST_SUITE("BuildRaceEndReason - fallback") {

TEST_CASE("no flags, not won, not lost -> generic fallback") {
    RaceEndContext ctx;
    CHECK(BuildRaceEndReason(ctx) == "RACE OVER");
}

} // TEST_SUITE
