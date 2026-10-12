#include "doctest/doctest.h"

#include "core/game_clock.h"
#include "race/grudge.h"

#include <array>
#include <cstdlib>

TEST_CASE("grudge night runs on Wednesdays and nothing else") {
    // Day 0 of the calendar is Sunday 16 June 1963, so Wednesday is day 3 and
    // every seventh day after it.
    CHECK_FALSE(srrace::GrudgeNightOpen(0));   // Sunday
    CHECK_FALSE(srrace::GrudgeNightOpen(1));
    CHECK_FALSE(srrace::GrudgeNightOpen(2));
    CHECK(srrace::GrudgeNightOpen(3));         // Wednesday
    CHECK_FALSE(srrace::GrudgeNightOpen(4));
    CHECK(srrace::GrudgeNightOpen(10));
    CHECK(srrace::GrudgeNightOpen(17));

    int open = 0;
    for (int d = 0; d < 70; ++d) {
        if (srrace::GrudgeNightOpen(d)) ++open;
    }
    CHECK(open == 10);   // one a week over ten weeks

    // A clock that has not started is a Sunday, not a Wednesday.
    CHECK_FALSE(srrace::GrudgeNightOpen(GameClock::DayNumber(0)));
}

TEST_CASE("the opening round is type A and the rest type B") {
    CHECK(srrace::GrudgeRaceTypeForRound(0) == srrace::GRUDGE_RACE_TYPE_A);
    for (int r = 1; r < srrace::GRUDGE_MAX_ROUNDS; ++r) {
        CHECK(srrace::GrudgeRaceTypeForRound(r) == srrace::GRUDGE_RACE_TYPE_B);
    }
    // The engine tests the two together as `race_type & 0x0c`, which has to
    // catch either.
    for (int r = 0; r < srrace::GRUDGE_MAX_ROUNDS; ++r) {
        CHECK((srrace::GrudgeRaceTypeForRound(r) & 0x0c) != 0);
    }
}

TEST_CASE("a round walks both drivers, and the walk clamps") {
    srrace::GrudgeStats s;
    s.playerSkill = 4;
    s.playerReflex = 4;
    s.opponentSkill = 4;
    s.opponentReflex = 4;

    // The winner's number goes UP and the loser's down.
    srrace::GrudgeApplyRoundResult(s, {true, true});
    CHECK(s.playerSkill == 5);
    CHECK(s.playerReflex == 5);
    CHECK(s.opponentSkill == 3);
    CHECK(s.opponentReflex == 3);

    srrace::GrudgeApplyRoundResult(s, {false, false});
    CHECK(s.playerSkill == 4);
    CHECK(s.playerReflex == 4);
    CHECK(s.opponentSkill == 4);
    CHECK(s.opponentReflex == 4);

    // Neither end escapes 1..8, however long the night runs.
    for (int i = 0; i < 40; ++i) {
        srrace::GrudgeApplyRoundResult(s, {true, true});
    }
    CHECK(s.playerSkill == srrace::GRUDGE_STAT_MAX);
    CHECK(s.playerReflex == srrace::GRUDGE_STAT_MAX);
    CHECK(s.opponentSkill == srrace::GRUDGE_STAT_MIN);
    CHECK(s.opponentReflex == srrace::GRUDGE_STAT_MIN);

    for (int i = 0; i < 40; ++i) {
        srrace::GrudgeApplyRoundResult(s, {false, false});
    }
    CHECK(s.playerSkill == srrace::GRUDGE_STAT_MIN);
    CHECK(s.opponentSkill == srrace::GRUDGE_STAT_MAX);
}

TEST_CASE("skill and reflex are judged separately") {
    // The two steps key off different flags in the original, so a round can
    // move one and leave the other going the other way.
    srrace::GrudgeStats s;
    s.playerSkill = 4;
    s.playerReflex = 4;
    s.opponentSkill = 4;
    s.opponentReflex = 4;

    srrace::GrudgeApplyRoundResult(s, {/*skill*/ true, /*reflex*/ false});
    CHECK(s.playerSkill == 5);
    CHECK(s.opponentSkill == 3);
    CHECK(s.playerReflex == 3);
    CHECK(s.opponentReflex == 5);
}

TEST_CASE("a loss puts you out only in the first two rounds") {
    CHECK(srrace::GrudgeLossEliminates(0));
    CHECK(srrace::GrudgeLossEliminates(1));
    for (int r = 2; r < srrace::GRUDGE_MAX_ROUNDS; ++r) {
        CHECK_FALSE(srrace::GrudgeLossEliminates(r));
    }
}

TEST_CASE("the gates are the entry fee and the tank") {
    // Both read off the original; the garage applies them together with the
    // day check before the night starts.
    CHECK(srrace::GRUDGE_ENTRY_FEE == 30);
    CHECK(srrace::GRUDGE_MIN_GAS == 24);
    CHECK(srrace::GRUDGE_MAX_ROUNDS == 4);
}


TEST_CASE("only the top three placings pay") {
    CHECK(srrace::GrudgePayout(0) == 150);
    CHECK(srrace::GrudgePayout(1) == 100);
    CHECK(srrace::GrudgePayout(2) == 50);
    for (int p = 3; p < 10; ++p) {
        CHECK(srrace::GrudgePayout(p) == 0);
    }
    CHECK(srrace::GrudgePayout(-1) == 0);

    // Third place still clears the entry fee, second doubles it.
    CHECK(srrace::GrudgePayout(2) > srrace::GRUDGE_ENTRY_FEE);
}


TEST_CASE("running faster than the breakout is a disqualification") {
    const int breakout = 98;   // 9.8, the observed qualifier

    // The winning times from a played night all sit above it.
    for (int t : {113, 130, 155, 138}) {
        CHECK_FALSE(srrace::GrudgeBrokeOut(t, breakout));
    }
    // Quicker than the limit is out; matching it exactly is not.
    CHECK(srrace::GrudgeBrokeOut(97, breakout));
    CHECK_FALSE(srrace::GrudgeBrokeOut(98, breakout));
}


TEST_CASE("a qualifier can be refused for being too slow") {
    // 80.1 seconds. Under it the time stands as the breakout, at or over it the
    // night is over on message 56 however the race went.
    CHECK(srrace::GRUDGE_BREAKOUT_MAX_TENTHS == 801);
    CHECK_FALSE(srrace::GrudgeBreakoutTooWeak(800));
    CHECK(srrace::GrudgeBreakoutTooWeak(801));

    // Every time from a played night stands.
    for (int t : {98, 113, 130, 155, 138}) {
        CHECK_FALSE(srrace::GrudgeBreakoutTooWeak(t));
    }

    srrace::GrudgeNightState slow;
    srrace::GrudgeQualified(slow, true, 900);
    CHECK(slow.outcome == srrace::GrudgeOutcome::WeakBreakout);
    CHECK(srrace::GrudgeNextStep(slow) == srrace::GrudgeStep::Over);
    CHECK(srrace::GrudgeWinnings(slow) == 0);
    CHECK(slow.round == -1);            // never got to a round
    CHECK(slow.breakoutTenths == 0);    // and never set a breakout

    srrace::GrudgeNightState ok;
    srrace::GrudgeQualified(ok, true, 800);
    CHECK(ok.outcome == srrace::GrudgeOutcome::Racing);
    CHECK(ok.breakoutTenths == 800);
}

TEST_CASE("the opponent breaking out hands you the round") {
    // No breakout on record is not a breakout, however quick the time.
    CHECK_FALSE(srrace::GrudgeOpponentBrokeOut(50, 0));
    CHECK(srrace::GrudgeOpponentBrokeOut(95, 100));
    CHECK_FALSE(srrace::GrudgeOpponentBrokeOut(100, 100));   // matching is not under

    const srrace::GrudgeRoundResult lost{false, false, 0};
    const srrace::GrudgeRoundResult lostTheyBroke{false, false, 130};

    // Round 0: losing normally is elimination, but not when they broke out.
    srrace::GrudgeNightState out;
    srrace::GrudgeQualified(out, true, 98);
    srrace::GrudgeRoundDone(out, lost, 120, false);
    CHECK(out.outcome == srrace::GrudgeOutcome::Eliminated);

    srrace::GrudgeNightState saved;
    srrace::GrudgeQualified(saved, true, 98);
    srrace::GrudgeRoundDone(saved, lostTheyBroke, 120, false);
    CHECK(saved.outcome == srrace::GrudgeOutcome::Racing);
    CHECK(saved.round == 1);
    // And the promotion walks the stats the winner's way.
    CHECK(saved.stats.playerSkill == 5);
    CHECK(saved.stats.opponentSkill == 3);

    // Your own breakout still comes first: quicker than it and you are out
    // whatever the other driver's number was.
    srrace::GrudgeNightState mine;
    srrace::GrudgeQualified(mine, true, 98);
    srrace::GrudgeRoundDone(mine, lostTheyBroke, 90, false);
    CHECK(mine.outcome == srrace::GrudgeOutcome::BrokeOut);

    // And a time over their number is an ordinary loss.
    srrace::GrudgeNightState plain;
    srrace::GrudgeQualified(plain, true, 98);
    srrace::GrudgeRoundDone(plain, {false, false, 110}, 120, false);
    CHECK(plain.outcome == srrace::GrudgeOutcome::Eliminated);
}

TEST_CASE("the bracket handicap holds the quicker dial back") {
    CHECK(srrace::GRUDGE_STAGE_FRAMES == 110);
    CHECK(srrace::GRUDGE_HANDICAP_FRAMES_PER_TENTH == 7);

    // Equal dials, no head start either way.
    auto even = srrace::GrudgeStageDelays(120, 120);
    CHECK(even.playerFrames == 110);
    CHECK(even.opponentFrames == 110);

    // The player's dial is quicker (the lower number), so the player waits.
    auto playerQuicker = srrace::GrudgeStageDelays(100, 130);
    CHECK(playerQuicker.playerFrames == 110 + 30 * 7);
    CHECK(playerQuicker.opponentFrames == 110);

    // And the other way round.
    auto oppQuicker = srrace::GrudgeStageDelays(130, 100);
    CHECK(oppQuicker.playerFrames == 110);
    CHECK(oppQuicker.opponentFrames == 110 + 30 * 7);

    // The head start is always the whole difference, whoever holds it.
    for (int p : {98, 113, 130, 155}) {
        for (int o : {98, 113, 130, 155}) {
            auto d = srrace::GrudgeStageDelays(p, o);
            const int gap = (d.playerFrames - d.opponentFrames);
            CHECK(std::abs(gap) == std::abs(o - p) * 7);
            // Neither ever launches before the base stage time.
            CHECK(d.playerFrames >= srrace::GRUDGE_STAGE_FRAMES);
            CHECK(d.opponentFrames >= srrace::GRUDGE_STAGE_FRAMES);
        }
    }
}

TEST_CASE("the predicted time table has all 21 cases") {
    // Five cars ignore the driver entirely, two more are flat constants, and
    // anything off the end falls to the default.
    for (int flat : {0, 1, 7, 9, 0x12}) {
        CHECK(srrace::GrudgePredictedTime(flat, 0) == 158);
        CHECK(srrace::GrudgePredictedTime(flat, 8) == 158);
    }
    CHECK(srrace::GrudgePredictedTime(10, 4) == 122);
    CHECK(srrace::GrudgePredictedTime(0x14, 4) == 90);
    CHECK(srrace::GrudgePredictedTime(-1, 4) == 71);
    CHECK(srrace::GrudgePredictedTime(srrace::GRUDGE_CAR_CASES, 4) == 71);

    // The two with a step in them: case 2 and case 13 both branch on b10 < 6,
    // i.e. on driverByte > 4.
    CHECK(srrace::GrudgePredictedTime(0xd, 5) == 88);        // b10 = 5, no step
    CHECK(srrace::GrudgePredictedTime(0xd, 4) == 94);        // b10 = 6, +6
    CHECK(srrace::GrudgePredictedTime(2, 5) == 5 * 3 + 121);
    CHECK(srrace::GrudgePredictedTime(2, 4) == (-4 + 5) * 4 + 6 * 3 + 121);

    // A linear one, both ways.
    CHECK(srrace::GrudgePredictedTime(3, 0) == 10 * 4 + 103);
    CHECK(srrace::GrudgePredictedTime(3, 8) == 2 * 4 + 103);

    // What lands in the record is scaled by 10/6, and every case stays inside
    // the range a breakout is allowed to be.
    CHECK(srrace::GrudgeBreakoutForCar(10, 4) == (122 * 10) / 6);
    for (int c = 0; c < srrace::GRUDGE_CAR_CASES; ++c) {
        for (int d = 0; d <= 8; ++d) {
            const int b = srrace::GrudgeBreakoutForCar(c, d);
            CHECK(b > 0);
            CHECK_FALSE(srrace::GrudgeBreakoutTooWeak(b));
        }
    }
}

TEST_CASE("seeding sorts the field then pairs top half against bottom") {
    srrace::GrudgeFieldEntry field[srrace::GRUDGE_FIELD_SIZE];
    // Hand them in backwards, so the sort has to do real work. driver doubles
    // as the label: after sorting, driver 0 is the quickest dial.
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
        field[i].driver = srrace::GRUDGE_FIELD_SIZE - 1 - i;
        field[i].breakoutTenths = 100 + field[i].driver;
    }
    srrace::GrudgeSeedBracket(field, srrace::GRUDGE_FIELD_SIZE);

    // 0,5,1,6,2,7,3,8,4,9 - first meets sixth, second meets seventh, and so on.
    const int expect[srrace::GRUDGE_FIELD_SIZE] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
        CHECK(field[i].driver == expect[i]);
        // The breakout travels with its driver.
        CHECK(field[i].breakoutTenths == 100 + expect[i]);
    }

    // Already sorted in gives the same pairing, so the shuffle is the only
    // thing deciding it.
    srrace::GrudgeFieldEntry sorted[srrace::GRUDGE_FIELD_SIZE];
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
        sorted[i].driver = i;
        sorted[i].breakoutTenths = 100 + i;
    }
    srrace::GrudgeSeedBracket(sorted, srrace::GRUDGE_FIELD_SIZE);
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
        CHECK(sorted[i].driver == expect[i]);
    }

    // Nothing is lost or duplicated.
    int seen[srrace::GRUDGE_FIELD_SIZE] = {};
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) ++seen[field[i].driver];
    for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) CHECK(seen[i] == 1);

    // Degenerate sizes are left alone rather than walked off the end.
    srrace::GrudgeFieldEntry one[1];
    one[0].driver = 7;
    srrace::GrudgeSeedBracket(one, 1);
    CHECK(one[0].driver == 7);
    srrace::GrudgeSeedBracket(nullptr, srrace::GRUDGE_FIELD_SIZE);
}

TEST_CASE("the bracket advances and the opponent changes every round") {
    auto makeField = []() {
        std::array<srrace::GrudgeFieldEntry, srrace::GRUDGE_FIELD_SIZE> f{};
        for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
            f[i].driver = i;                     // 0..8 plus the player at 9
            f[i].breakoutTenths = 100 + i;
        }
        f[srrace::GRUDGE_FIELD_SIZE - 1].driver = srrace::GRUDGE_PLAYER;
        srrace::GrudgeSeedBracket(f.data(), srrace::GRUDGE_FIELD_SIZE);
        return f;
    };

    auto field = makeField();
    const int n = srrace::GRUDGE_FIELD_SIZE;

    // The player is found by their negative driver, and paired with the other
    // half of their pair - an even slot with the odd one after it.
    const int me = srrace::GrudgePlayerSlot(field.data(), n);
    REQUIRE(me >= 0);
    const int first = srrace::GrudgeOpponentSlot(field.data(), n);
    REQUIRE(first >= 0);
    CHECK(first != me);
    CHECK((me / 2) == (first / 2));

    // Win a round: that opponent goes out, the other pairs are drawn, and the
    // next opponent is somebody else who is still in.
    srrace::RaceRng rng(0xC0FFEEu);
    srrace::GrudgeAdvanceBracket(field.data(), n, 0, true, rng);
    CHECK(field[first].placing == srrace::GRUDGE_OUT);
    CHECK(field[me].placing == srrace::GRUDGE_IN);
    const int second = srrace::GrudgeOpponentSlot(field.data(), n);
    REQUIRE(second >= 0);
    CHECK(second != first);
    CHECK(field[second].placing == srrace::GRUDGE_IN);

    // Round 0 takes out the player's opponent and one other pair's loser, so eight are left.
    auto stillIn = [&]() {
        int c = 0;
        for (int i = 0; i < n; ++i) {
            if (field[i].placing == srrace::GRUDGE_IN) ++c;
        }
        return c;
    };
    CHECK(stillIn() == 8);
    srrace::GrudgeAdvanceBracket(field.data(), n, 1, true, rng);
    CHECK(stillIn() == 4);
    CHECK(field[me].placing == srrace::GRUDGE_IN);
    CHECK(srrace::GrudgeOpponentSlot(field.data(), n) >= 0);
    srrace::GrudgeAdvanceBracket(field.data(), n, 2, true, rng);
    CHECK(stillIn() == 2);
    CHECK(srrace::GrudgeOpponentSlot(field.data(), n) >= 0);
    srrace::GrudgeAdvanceBracket(field.data(), n, 3, true, rng);
    CHECK(stillIn() == 1);
    CHECK(field[me].placing == srrace::GRUDGE_IN);   // the player won it
    CHECK(srrace::GrudgePlayerPlacing(field.data(), n) == 0);

    // From round 2 the loser is marked as knocked out LATE, which is what tells
    // second and third from the rest.
    auto late = makeField();
    const int lateMe = srrace::GrudgePlayerSlot(late.data(), n);
    const int lateThem = srrace::GrudgeOpponentSlot(late.data(), n);
    srrace::GrudgeAdvanceBracket(late.data(), n, 2, true, rng);
    CHECK(late[lateThem].placing == srrace::GRUDGE_OUT_LATE);
    CHECK(late[lateMe].placing == srrace::GRUDGE_IN);

    // Losing puts the player out, and their record stays put so the standings
    // can still name them.
    auto lost = makeField();
    const int lostMe = srrace::GrudgePlayerSlot(lost.data(), n);
    srrace::GrudgeAdvanceBracket(lost.data(), n, 0, false, rng);
    CHECK(lost[lostMe].placing == srrace::GRUDGE_OUT);
    CHECK(lost[lostMe].driver == srrace::GRUDGE_PLAYER);
    CHECK(srrace::GrudgeOpponentSlot(lost.data(), n) == -1);

    // Degenerate inputs are left alone.
    srrace::GrudgeAdvanceBracket(nullptr, n, 0, true, rng);
    CHECK(srrace::GrudgePlayerSlot(nullptr, n) == -1);
    CHECK(srrace::GrudgeOpponentSlot(nullptr, n) == -1);
}

TEST_CASE("an unfilled slot is not mistaken for the player") {
    // A default constructed field has nobody in it, and the player is found by an exact -1.
    std::array<srrace::GrudgeFieldEntry, srrace::GRUDGE_FIELD_SIZE> empty{};
    const int n = srrace::GRUDGE_FIELD_SIZE;
    for (const auto& e : empty) {
        CHECK(e.driver == srrace::GRUDGE_EMPTY);
    }
    CHECK(srrace::GrudgePlayerSlot(empty.data(), n) == -1);
    CHECK(srrace::GrudgeOpponentSlot(empty.data(), n) == -1);

    // A short field: two real drivers and the player, the rest unfilled. The
    // player pairs with a real driver, never with an empty slot.
    std::array<srrace::GrudgeFieldEntry, srrace::GRUDGE_FIELD_SIZE> few{};
    few[0].driver = 3;
    few[1].driver = srrace::GRUDGE_PLAYER;
    few[2].driver = 4;
    CHECK(srrace::GrudgePlayerSlot(few.data(), n) == 1);
    CHECK(srrace::GrudgeOpponentSlot(few.data(), n) == 0);

    // Advancing does not hand a win to an empty slot.
    srrace::RaceRng rng(5u);
    srrace::GrudgeAdvanceBracket(few.data(), n, 1, true, rng);
    CHECK(few[0].placing == srrace::GRUDGE_OUT);
    CHECK(few[1].placing == srrace::GRUDGE_IN);
    for (int i = 3; i < n; ++i) {
        CHECK(few[i].placing == srrace::GRUDGE_IN);   // untouched, still empty
        CHECK(few[i].driver == srrace::GRUDGE_EMPTY);
    }
}

TEST_CASE("the gates let you in only on a Wednesday with fuel and the fee") {
    CHECK(srrace::GrudgeCanEnter(3, 24, 30));
    CHECK_FALSE(srrace::GrudgeCanEnter(4, 24, 30));    // Thursday
    CHECK_FALSE(srrace::GrudgeCanEnter(3, 23, 30));    // short of fuel
    CHECK_FALSE(srrace::GrudgeCanEnter(3, 24, 29));    // short of the fee
}

TEST_CASE("a night played through pays first place") {
    // The observed night: 9.8 qualifier, then four wins above it.
    srrace::GrudgeNightState n;
    CHECK(srrace::GrudgeNextStep(n) == srrace::GrudgeStep::Qualify);

    srrace::GrudgeQualified(n, true, 98);
    CHECK(n.breakoutTenths == 98);
    CHECK(n.round == 0);
    CHECK(srrace::GrudgeNextStep(n) == srrace::GrudgeStep::Race);

    const int times[4] = {113, 130, 155, 138};
    for (int i = 0; i < 4; ++i) {
        srrace::GrudgeRoundDone(n, {true, true}, times[i], false);
    }
    CHECK(n.outcome == srrace::GrudgeOutcome::Finished);
    CHECK(srrace::GrudgeNextStep(n) == srrace::GrudgeStep::Standings);
    CHECK(srrace::GrudgeWinnings(n) == 150);
}

TEST_CASE("the qualifier is a solo run, so only a failure ends it") {
    // The original runs it against sentinel opponent slot 21, which FUN_0025001a flags as a solo run.
    srrace::GrudgeNightState done;
    srrace::GrudgeQualified(done, /*completed*/ true, 120);
    CHECK(done.outcome == srrace::GrudgeOutcome::Racing);
    CHECK(done.round == 0);
    CHECK(done.breakoutTenths == 120);

    // A wreck, a blown engine or a jumped start is the only way out of it.
    srrace::GrudgeNightState wrecked;
    srrace::GrudgeQualified(wrecked, /*completed*/ false, 120);
    CHECK(wrecked.outcome == srrace::GrudgeOutcome::Eliminated);
    CHECK(wrecked.round == -1);
    CHECK(srrace::GrudgeWinnings(wrecked) == 0);
}

TEST_CASE("failing the qualifier ends the night before any round") {
    srrace::GrudgeNightState n;
    srrace::GrudgeQualified(n, false, 98);
    CHECK(n.outcome == srrace::GrudgeOutcome::Eliminated);
    CHECK(srrace::GrudgeNextStep(n) == srrace::GrudgeStep::Over);
    CHECK(srrace::GrudgeWinnings(n) == 0);
}

TEST_CASE("breaking out ends the night whatever else happened") {
    srrace::GrudgeNightState n;
    srrace::GrudgeQualified(n, true, 98);
    // Quicker than the breakout, even though the race was won.
    srrace::GrudgeRoundDone(n, {true, true}, 90, false);
    CHECK(n.outcome == srrace::GrudgeOutcome::BrokeOut);
    CHECK(srrace::GrudgeWinnings(n) == 0);

    srrace::GrudgeNightState m;
    srrace::GrudgeQualified(m, true, 98);
    srrace::GrudgeRoundDone(m, {true, true}, 120, true);
    CHECK(m.outcome == srrace::GrudgeOutcome::CrossedLine);
}

TEST_CASE("an early loss is elimination, a late one races on for third") {
    srrace::GrudgeNightState early;
    srrace::GrudgeQualified(early, true, 98);
    CHECK_FALSE(srrace::GrudgeRoundDone(early, {false, false}, 120, false));
    CHECK(early.outcome == srrace::GrudgeOutcome::Eliminated);

    // FUN_0025558a keeps looping after a loss from round 2 on, so the round
    // still advances and the night ends at the standings.
    srrace::GrudgeNightState late;
    srrace::GrudgeQualified(late, true, 98);
    CHECK(srrace::GrudgeRoundDone(late, {true, true}, 120, false));
    CHECK(srrace::GrudgeRoundDone(late, {true, true}, 120, false));
    CHECK(late.round == 2);
    CHECK_FALSE(srrace::GrudgeRoundDone(late, {false, false}, 120, false));
    CHECK(late.round == 3);
    CHECK(late.outcome == srrace::GrudgeOutcome::Racing);
    srrace::GrudgeRoundDone(late, {true, true}, 120, false);
    CHECK(late.outcome == srrace::GrudgeOutcome::Finished);
}

TEST_CASE("the last round places the pairs that are left") {
    // The field after a night: the player and one other on 0 for the final,
    // two held at OUT_LATE for the consolation, the rest knocked out.
    auto makeFinal = []() {
        std::array<srrace::GrudgeFieldEntry, srrace::GRUDGE_FIELD_SIZE> f{};
        for (int i = 0; i < srrace::GRUDGE_FIELD_SIZE; ++i) {
            f[i].driver = i;
            f[i].placing = srrace::GRUDGE_OUT;
        }
        f[0].driver = srrace::GRUDGE_PLAYER;
        f[0].placing = srrace::GRUDGE_IN;
        f[1].placing = srrace::GRUDGE_IN;
        f[4].placing = srrace::GRUDGE_OUT_LATE;
        f[7].placing = srrace::GRUDGE_OUT_LATE;
        return f;
    };
    const int n = srrace::GRUDGE_FIELD_SIZE;
    srrace::RaceRng rng(0xBEEFu);

    // The final pairs the two on 0, not the whole survivor list.
    auto won = makeFinal();
    CHECK(srrace::GrudgeOpponentSlot(won.data(), n) == 1);
    srrace::GrudgeAdvanceBracket(won.data(), n, 3, true, rng);
    CHECK(srrace::GrudgePlayerPlacing(won.data(), n) == 0);
    CHECK(won[1].placing == 1);
    // And the consolation pair is settled the same round: third and fourth.
    CHECK(won[4].placing + won[7].placing == 2 + 3);

    auto lost = makeFinal();
    srrace::GrudgeAdvanceBracket(lost.data(), n, 3, false, rng);
    CHECK(srrace::GrudgePlayerPlacing(lost.data(), n) == 1);
    CHECK(lost[1].placing == srrace::GRUDGE_IN);

    // A player who lost round 2 races the consolation, for third or fourth.
    auto third = makeFinal();
    third[0].placing = srrace::GRUDGE_OUT_LATE;
    third[4].placing = srrace::GRUDGE_IN;
    CHECK(srrace::GrudgeOpponentSlot(third.data(), n) == 7);
    srrace::GrudgeAdvanceBracket(third.data(), n, 3, true, rng);
    CHECK(srrace::GrudgePlayerPlacing(third.data(), n) == 2);
    CHECK(third[7].placing == 3);

    auto fourth = makeFinal();
    fourth[0].placing = srrace::GRUDGE_OUT_LATE;
    fourth[4].placing = srrace::GRUDGE_IN;
    srrace::GrudgeAdvanceBracket(fourth.data(), n, 3, false, rng);
    CHECK(srrace::GrudgePlayerPlacing(fourth.data(), n) == 3);

    // What each of those pays, which is the whole point of the marks.
    CHECK(srrace::GrudgePayout(srrace::GrudgePlayerPlacing(won.data(), n)) == 150);
    CHECK(srrace::GrudgePayout(srrace::GrudgePlayerPlacing(lost.data(), n)) == 100);
    CHECK(srrace::GrudgePayout(srrace::GrudgePlayerPlacing(third.data(), n)) == 50);
    CHECK(srrace::GrudgePayout(srrace::GrudgePlayerPlacing(fourth.data(), n)) == 0);

    // Knocked out before the placings were handed out pays nothing.
    auto out = makeFinal();
    out[0].placing = srrace::GRUDGE_OUT;
    CHECK(srrace::GrudgePlayerPlacing(out.data(), n) == srrace::GRUDGE_PAYING_PLACES);
}
