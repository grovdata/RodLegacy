#include <doctest/doctest.h>

#include "car_haggle.h"

// A $1000 car with roll20 = 0 has an opening ceiling of exactly $750, which
// keeps the numbers below easy to check by hand:
//   ceiling         = 1000 * 75 / 100 = 750
//   counter-offer 1 =  750 * 85 / 100 = 637 -> rounded to 635
//   counter-offer 2 =  750 * 91 / 100 = 682 -> rounded to 680
// He closes at anything whose 90% is within the current ceiling.

TEST_SUITE("CarHaggle - sanity ceilings") {

TEST_CASE("over 115% of value: you must be kidding, and the sale is over") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(1151, 0);
    CHECK(s.outcome == HaggleOutcome::Rejected);
    CHECK(s.message == HaggleMessage::MustBeKidding);
}

TEST_CASE("exactly 115% is not over the line") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(1150, 0);
    CHECK(s.message == HaggleMessage::NoWay);
    CHECK(s.outcome == HaggleOutcome::AskPrice);
}

TEST_CASE("over value but under 115%: no way, then one retry") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(1100, 0);
    CHECK(s.outcome == HaggleOutcome::AskPrice);
    CHECK(s.message == HaggleMessage::NoWay);

    SUBCASE("still over value on the retry: get lost") {
        HaggleStep t = h.SubmitPrice(1050, 0);
        CHECK(t.outcome == HaggleOutcome::Rejected);
        CHECK(t.message == HaggleMessage::GetLost);
    }

    SUBCASE("a sane price on the retry carries on into the haggle") {
        HaggleStep t = h.SubmitPrice(1000, 0);
        CHECK(t.outcome == HaggleOutcome::CounterOffer);
        CHECK(t.amount == 635);
    }
}

TEST_CASE("the ceilings are not re-tested on later prompts") {
    // This is what the $22,001 recipe relies on. Refuse both counter-offers
    // and the third prompt takes a number the first one would have thrown out.
    CarHaggle h(1000, 0);
    CHECK(h.SubmitPrice(1000, 0).outcome == HaggleOutcome::CounterOffer);
    CHECK(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);
    CHECK(h.SubmitPrice(1000, 0).outcome == HaggleOutcome::CounterOffer);
    CHECK(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);

    // 5000 is well over 115% of a $1000 car, but nothing checks that here.
    HaggleStep s = h.SubmitPrice(5000, 0);
    CHECK(s.outcome == HaggleOutcome::Rejected);
    CHECK(s.message == HaggleMessage::NoThanks);
}

} // TEST_SUITE

TEST_SUITE("CarHaggle - closing the deal") {

TEST_CASE("at or under the opening ceiling he buys straight away") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(750, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == 750);
    CHECK(s.message == HaggleMessage::IllTakeIt);
}

TEST_CASE("anything over the ceiling gets a counter-offer, never a sale") {
    // The final "90% of the price is within the ceiling" test is only reached
    // once the price is already inside the ceiling, so it never closes a deal
    // the ceiling would not have. The one prompt that skips that guard is the
    // third one - see the test below.
    CarHaggle h(1000, 0);
    CHECK(h.SubmitPrice(751, 0).outcome == HaggleOutcome::CounterOffer);
}

TEST_CASE("the last prompt is the one that pays over the ceiling") {
    // Two refusals leave the ceiling at 680, and the third prompt goes
    // straight to the final test: it closes at anything up to 680 / 0.9.
    auto reachThirdPrompt = [](CarHaggle& h) {
        h.SubmitPrice(1000, 0);
        h.AnswerCounterOffer(false);
        h.SubmitPrice(1000, 0);
        h.AnswerCounterOffer(false);
    };

    CarHaggle inside(1000, 0);
    reachThirdPrompt(inside);
    REQUIRE(inside.BuyerCeiling() == 680);
    HaggleStep s = inside.SubmitPrice(756, 0);  // 756 * 9 / 10 = 680
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == 756);

    CarHaggle outside(1000, 0);
    reachThirdPrompt(outside);
    HaggleStep t = outside.SubmitPrice(757, 0);  // 757 * 9 / 10 = 681
    CHECK(t.outcome == HaggleOutcome::Rejected);
    CHECK(t.message == HaggleMessage::NoThanks);
}

TEST_CASE("selling for under half the car's worth gets the other line") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(499, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.message == HaggleMessage::MustBeCrazy);

    CarHaggle h2(1000, 0);
    CHECK(h2.SubmitPrice(500, 0).message == HaggleMessage::IllTakeIt);
}

TEST_CASE("accepting a counter-offer closes at the counter-offer") {
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(1000, 0);
    REQUIRE(s.outcome == HaggleOutcome::CounterOffer);
    REQUIRE(s.amount == 635);

    HaggleStep t = h.AnswerCounterOffer(true);
    CHECK(t.outcome == HaggleOutcome::Sold);
    CHECK(t.amount == 635);
}

TEST_CASE("the second counter-offer beats the first") {
    CarHaggle h(1000, 0);
    CHECK(h.SubmitPrice(1000, 0).amount == 635);
    CHECK(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);

    HaggleStep s = h.SubmitPrice(1000, 0);
    CHECK(s.outcome == HaggleOutcome::CounterOffer);
    CHECK(s.amount == 680);
    CHECK(h.AnswerCounterOffer(true).amount == 680);
}

TEST_CASE("counter-offers are rounded down to whole fives") {
    // 100 * 85 / 100 = 85 already; use a value whose product is not a multiple
    // of 5: ceiling 749, first counter 636 -> 635.
    CarHaggle h(999, 0);
    CHECK(h.BuyerCeiling() == 749);
    CHECK(h.SubmitPrice(999, 0).amount == 635);
}

TEST_CASE("dropping under the ceiling on a later prompt still sells") {
    CarHaggle h(1000, 0);
    CHECK(h.SubmitPrice(1000, 0).outcome == HaggleOutcome::CounterOffer);
    CHECK(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);

    // 635 is the ceiling the refused offer left behind.
    HaggleStep s = h.SubmitPrice(600, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == 600);
}

} // TEST_SUITE

TEST_SUITE("CarHaggle - the 16-bit money cheat") {

// price * 9 is a word multiply, so a price whose ninefold wraps past 65536
// lands on a tiny remainder and sails through the final test. The windows
// open at 7282, 14564, 21846 and 29128.

TEST_CASE("$22,001 sells a $1000 car for $22,001") {
    CarHaggle h(1000, 0);
    REQUIRE(h.SubmitPrice(1000, 0).outcome == HaggleOutcome::CounterOffer);
    REQUIRE(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);
    REQUIRE(h.SubmitPrice(1000, 0).outcome == HaggleOutcome::CounterOffer);
    REQUIRE(h.AnswerCounterOffer(false).outcome == HaggleOutcome::AskPrice);

    // 22001 * 9 = 198009; 198009 mod 65536 = 1401; 1401 / 10 = 140 <= 635.
    HaggleStep s = h.SubmitPrice(22001, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == 22001);
    CHECK(s.message == HaggleMessage::IllTakeIt);
}

TEST_CASE("the first wrap window opens at 7282") {
    auto reachThirdPrompt = [](CarHaggle& h) {
        h.SubmitPrice(1000, 0);
        h.AnswerCounterOffer(false);
        h.SubmitPrice(1000, 0);
        h.AnswerCounterOffer(false);
    };

    CarHaggle inside(1000, 0);
    reachThirdPrompt(inside);
    CHECK(inside.SubmitPrice(7282, 0).outcome == HaggleOutcome::Sold);

    CarHaggle outside(1000, 0);
    reachThirdPrompt(outside);
    CHECK(outside.SubmitPrice(7281, 0).outcome == HaggleOutcome::Rejected);
}

TEST_CASE("a price over 32767 pays out negative") {
    CarHaggle h(1000, 0);
    h.SubmitPrice(1000, 0);
    h.AnswerCounterOffer(false);
    h.SubmitPrice(1000, 0);
    h.AnswerCounterOffer(false);

    // 36410 * 9 = 327690; mod 65536 = 6, so the test passes - but the payout
    // is a sign-extended word.
    HaggleStep s = h.SubmitPrice(36410, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == -29126);
}

TEST_CASE("the typed number itself wraps at 16 bits") {
    // 65536 + 750 reads back as 750, which this car's buyer accepts.
    CarHaggle h(1000, 0);
    HaggleStep s = h.SubmitPrice(66286, 0);
    CHECK(s.outcome == HaggleOutcome::Sold);
    CHECK(s.amount == 750);
}

} // TEST_SUITE

TEST_SUITE("CarHaggle - the opening ceiling") {

TEST_CASE("roll20 moves it across 75-94% of value") {
    CHECK(CarHaggle(1000, 0).BuyerCeiling() == 750);
    CHECK(CarHaggle(1000, 19).BuyerCeiling() == 940);
}

} // TEST_SUITE
