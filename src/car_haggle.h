#pragma once

// Amiga car-sale haggle, ported from SellOrSwitchCarLoop (0x00242ba4) and
// ShowSellCounterOffer (0x00242a8e). Full write-up, including where the
// constants come from, in RodLegacyResearch/docs/gameplay.md ("Selling").
//
// The buyer has a hidden ceiling of 75-94% of the car's value. You name a
// price; if it is over the ceiling he counters with a lower one, and every
// refusal buys you another prompt - three in all. He closes at anything up to
// ceiling / 0.9.
//
// The two sanity ceilings (115% and 100% of the car's value) are tested only
// on the *first* price. Later prompts take any number, which is the hole the
// $22,001 trick goes through: the final test computes price * 9 in a 16-bit
// register, so a price whose ninefold wraps past 65536 lands on a tiny
// remainder and passes. That arithmetic is reproduced here on purpose - see
// Settle() in the .cpp.
//
// Rolls are passed in rather than drawn here, so the whole thing is a pure
// state machine that unit tests can drive (same approach as bet_acceptance.h).

enum class HaggleOutcome {
    AskPrice,      // prompt the player for a price, then call SubmitPrice
    CounterOffer,  // "How about $amount ?", then call AnswerCounterOffer
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

    // The price the player typed. Truncated to 16 bits first, exactly as the
    // original's numeric entry does.
    //
    // roll5 is rand(5), i.e. 0..4, and is only used when this call produces a
    // counter-offer. The original draws it at that point rather than up front;
    // drawing it eagerly here costs one extra number from the caller's
    // generator and nothing else.
    HaggleStep SubmitPrice(int price, int roll5);

    // Answer the counter-offer the last step returned.
    HaggleStep AnswerCounterOffer(bool accepted);

    // What the buyer will currently pay. Exposed for tests.
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
