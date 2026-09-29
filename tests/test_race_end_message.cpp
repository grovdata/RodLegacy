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
    CHECK(BuildRaceEndReason(ctx).text == "You have been fined $50 for speeding.");
}

TEST_CASE("cop caught, cannot afford the fine") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT;
    ctx.speedingFine = 50;
    ctx.bankroll = 49;
    CHECK(BuildRaceEndReason(ctx).text == "You've been sentenced for debts, Ace`!");
}

TEST_CASE("cop caught takes priority over a simultaneous playerLost/win flag") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT;
    ctx.speedingFine = 0;
    ctx.bankroll = 0;
    ctx.playerLost = true;
    CHECK(BuildRaceEndReason(ctx).text == "You have been fined $0 for speeding.");
}

TEST_CASE("hit the NPC") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_HIT_NPC;
    ctx.crashSpeed = 20;
    CHECK(BuildRaceEndReason(ctx).text == "Lucky you're so slow. You only lost\nthe race.");
}

TEST_CASE("hit the wall") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.crashSpeed = 20;
    CHECK(BuildRaceEndReason(ctx).text == "Lucky you're so slow. You only lost\nthe race.");
}

TEST_CASE("engine blown") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_ENGINE_BLOWN;
    CHECK(BuildRaceEndReason(ctx).text == "Blew your engine again, huh, Ace`?");
}

TEST_CASE("transmission dropped") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_TRANS_DROPPED;
    CHECK(BuildRaceEndReason(ctx).text == "You just dropped your`transmission,klutz`!");
}

TEST_CASE("stalled / jumped the light") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_STALLED;
    CHECK(BuildRaceEndReason(ctx).text == "Ya jumped the light, speedy`!");
}

}

TEST_SUITE("BuildRaceEndReason - wins") {

TEST_CASE("pink slip win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.isPinkSlip = true;
    ctx.betAmount = 0;  // pink slip races carry no cash stake
    CHECK(BuildRaceEndReason(ctx).text == "You just won a helluva car.");
}

TEST_CASE("$10 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 10;
    CHECK(BuildRaceEndReason(ctx).text == "Big deal. Ten lousy bucks.");
}

TEST_CASE("$25 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 25;
    CHECK(BuildRaceEndReason(ctx).text == "Big deal. Twenty-five bucks.");
}

TEST_CASE("$50 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 50;
    CHECK(BuildRaceEndReason(ctx).text == "Fifty smackers. Not bad.");
}

TEST_CASE("$100 win") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 100;
    CHECK(BuildRaceEndReason(ctx).text == "A hundred smackers, hot shot.");
}

TEST_CASE("win with no bet (just for kicks)") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 0;
    CHECK(BuildRaceEndReason(ctx).text == "You won! Too bad you didn't bet, though.");
}

TEST_CASE("win with an unlisted bet amount falls back to the no-bet line") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.betAmount = 999;
    CHECK(BuildRaceEndReason(ctx).text == "You won! Too bad you didn't bet, though.");
}

}

TEST_SUITE("BuildRaceEndReason - losses") {

TEST_CASE("a plain loss is the same line on the strip and the road") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.betAmount = 25;
    ctx.isDragRace = true;
    CHECK(BuildRaceEndReason(ctx).text == "You lose, sucker.");
    ctx.isDragRace = false;
    CHECK(BuildRaceEndReason(ctx).text == "You lose, sucker.");
    ctx.tiresBlown = true;
    CHECK(BuildRaceEndReason(ctx).text == "You lose, sucker.");
}

TEST_CASE("a plain pink-slip loss") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isPinkSlip = true;
    CHECK(BuildRaceEndReason(ctx).text == "Take a walk, sucker.");
}

TEST_CASE("losing to The King") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.vsKing = true;
    ctx.isDragRace = true;
    CHECK(BuildRaceEndReason(ctx).text == "Close, but no cigar punk`!");
    ctx.isDragRace = false;
    ctx.isPinkSlip = true;
    CHECK(BuildRaceEndReason(ctx).text ==
          "I`knew`you'd`choke`when`the`pressure`got`to`ya.\nBetter luck next time, sucker`!");
}

TEST_CASE("beating The King") {
    RaceEndContext ctx;
    ctx.playerWon = true;
    ctx.vsKing = true;
    ctx.isDragRace = true;
    CHECK(BuildRaceEndReason(ctx).text == "Beginner's luck, punk`!");
    ctx.isDragRace = false;
    ctx.isPinkSlip = true;
    CHECK(BuildRaceEndReason(ctx).text ==
          "You ran a good race.\nYou deserve to be King of the County`!");
}

TEST_CASE("a broken drivetrain in a pink-slip race loses the car too") {
    RaceEndContext ctx;
    ctx.playerLost = true;
    ctx.isPinkSlip = true;
    ctx.endFlags = srrace::END_ENGINE_BLOWN;
    CHECK(BuildRaceEndReason(ctx).text == "Blew your engine again`?\nYou lose. Take a walk, sucker.");
    ctx.endFlags = srrace::END_TRANS_DROPPED;
    CHECK(BuildRaceEndReason(ctx).text ==
          "You just dropped your`transmission`!\nYou lose. Take a walk, sucker.");
    ctx.isPinkSlip = false;
    CHECK(BuildRaceEndReason(ctx).text == "You just dropped your`transmission,klutz`!");
}

}

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
    CHECK(BuildRaceEndReason(ctx).text == "You lose. Take a walk, sucker.");
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
    CHECK(BuildRaceEndReason(ctx).text ==
          "Lucky you weren't killed, 'cause the car\nis totaled. You get $119 for scrap.");
}

TEST_CASE("the repair-quote tier has no bubble of its own") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.crashSpeed = 50;
    CHECK(BuildRaceEndReason(ctx).text.empty());
}

TEST_CASE("payout and quote arithmetic") {
    CHECK(srrace::JunkPayout(1000) == 400);
    CHECK(srrace::RepairRollRange(60) == 20);
    CHECK(srrace::RepairRollRange(61) == 40);
}

}

TEST_SUITE("SettlesBet - the cop and a jumped light return before the stake") {

using srrace::SettlesBet;

TEST_CASE("a ticket in a pink-slip race does not cost the car") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_COP_CAUGHT | srrace::END_FINISHED;
    ctx.isPinkSlip = true;
    ctx.playerLost = true;
    CHECK(SettlesBet(ctx) == false);
}

TEST_CASE("jumping the light costs neither money nor the car") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_STALLED | srrace::END_FINISHED;
    ctx.isPinkSlip = true;
    ctx.playerLost = true;
    CHECK(SettlesBet(ctx) == false);
}

TEST_CASE("every other loss settles normally") {
    RaceEndContext ctx;
    ctx.endFlags = srrace::END_WALL_HIT;
    ctx.isPinkSlip = true;
    CHECK(SettlesBet(ctx) == true);
    ctx.endFlags = 0;
    ctx.playerWon = true;
    CHECK(SettlesBet(ctx) == true);
}

}

TEST_SUITE("BuildRaceEndReason - fallback") {

TEST_CASE("no flags, not won, not lost -> no message") {
    RaceEndContext ctx;
    CHECK(BuildRaceEndReason(ctx).text == "");
}

}
