/*
 * RodLegacy - Car-sale haggle state machine: CarHaggle ports the original's ceilings, counter-offers and
 * 16-bit price quirk.
 *
 * Created: 2026-09-06
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

// The buyer has a hidden ceiling of 75-94% of the car's value. Each refusal of a counter-offer buys another
// prompt, three in all; he closes at anything up to ceiling / 0.9.
//
// The sanity ceilings (115% and 100% of value) only test the first price. The final test computes price * 9
// in 16 bits, reproduced on purpose: it is the $22,001 trick.
//
// Rolls are passed in so this is a pure, testable state machine.

enum class HaggleOutcome {
    AskPrice,      // prompt the player for a price, then call SubmitPrice
    CounterOffer,  // then call AnswerCounterOffer
    Sold,          // deal closed; amount is what the bankroll gains
    Rejected,      // no sale
};

enum class HaggleMessage {
    None,
    MustBeKidding,  // asked over 115% of the car's value
    NoWay,          // asked over the car's value; one retry follows
    GetLost,        // still over the car's value on the retry
    NoThanks,       // the final test failed
    IllTakeIt,      // sold
    MustBeCrazy,    // sold for less than half what the car is worth
};

struct HaggleStep {
    HaggleOutcome outcome;
    HaggleMessage message;
    // Counter-offer amount, or the sale price. Zero otherwise.
    int amount;
};

class CarHaggle {
   public:
    // carValue is what the car is worth; roll20 is rand(20), i.e. 0..19.
    CarHaggle(int carValue, int roll20);

    // Truncated to 16 bits first, as the original's numeric entry does. roll5 is only used for a counter-offer.
    HaggleStep SubmitPrice(int price, int roll5);

    HaggleStep AnswerCounterOffer(bool accepted);

    int BuyerCeiling() const { return offer; }

   private:
    enum class Stage {
        FirstPrice,
        OverValueRetry,  // the extra prompt after "No way!"
        Counter1,
        SecondPrice,
        Counter2,
        ThirdPrice,
        Ended,
    };

    HaggleStep AfterCaps(int roll5);
    HaggleStep AfterCounter1();
    HaggleStep AfterSecondPrice(int roll5);
    HaggleStep AfterCounter2();
    HaggleStep Settle();
    HaggleStep End(HaggleOutcome outcome, HaggleMessage message, int amount = 0);

    int value;  // the car's worth
    int base;   // the buyer's opening ceiling; both counter-offers derive from it
    int offer;  // the buyer's current ceiling
    int price;  // the player's current asking price, as a 16-bit word
    Stage stage;
};
