#include "car_haggle.h"

#include <cstdint>

// Round a counter-offer down to whole dollars-times-five, but never to
// nothing. ShowSellCounterOffer (0x00242a8e) does this in place, so the
// rounded figure is what the rest of the haggle compares against - which is
// why it only happens on the branch that actually shows an offer.
static int RoundOffer(int amount) {
    const int remainder = amount % 5;
    if (remainder == 0) return amount;
    amount -= remainder;
    return amount == 0 ? 5 : amount;
}

CarHaggle::CarHaggle(int carValue, int roll20)
    : value(carValue),
      base(carValue * (75 + roll20) / 100),
      offer(base),
      price(0),
      stage(Stage::FirstPrice) {}

HaggleStep CarHaggle::End(HaggleOutcome outcome, HaggleMessage message, int amount) {
    stage = Stage::Ended;
    return HaggleStep{outcome, message, amount};
}

HaggleStep CarHaggle::SubmitPrice(int rawPrice, int roll5) {
    price = static_cast<uint16_t>(rawPrice);

    switch (stage) {
        case Stage::FirstPrice:
            if (price > value * 115 / 100) {
                return End(HaggleOutcome::Rejected, HaggleMessage::MustBeKidding);
            }
            if (price > value) {
                stage = Stage::OverValueRetry;
                return HaggleStep{HaggleOutcome::AskPrice, HaggleMessage::NoWay, 0};
            }
            return AfterCaps(roll5);

        case Stage::OverValueRetry:
            if (price > value) {
                return End(HaggleOutcome::Rejected, HaggleMessage::GetLost);
            }
            return AfterCaps(roll5);

        case Stage::SecondPrice:
            return AfterSecondPrice(roll5);

        case Stage::ThirdPrice:
            return Settle();

        default:
            return End(HaggleOutcome::Rejected, HaggleMessage::None);
    }
}

// First counter-offer: 85-89% of the opening ceiling.
HaggleStep CarHaggle::AfterCaps(int roll5) {
    if (price > offer) {
        offer = base * (85 + roll5) / 100;
        if (price > offer) {
            offer = RoundOffer(offer);
            stage = Stage::Counter1;
            return HaggleStep{HaggleOutcome::CounterOffer, HaggleMessage::None, offer};
        }
    }
    return AfterCounter1();
}

HaggleStep CarHaggle::AfterCounter1() {
    if (price > offer) {
        stage = Stage::SecondPrice;
        return HaggleStep{HaggleOutcome::AskPrice, HaggleMessage::None, 0};
    }
    return Settle();
}

// Second counter-offer: 91-95% of the opening ceiling, so it is always the
// better of the two. Both derive from the opening ceiling, not from the
// current one, which is what makes refusing the first offer worthwhile.
HaggleStep CarHaggle::AfterSecondPrice(int roll5) {
    if (price > offer) {
        offer = base * (91 + roll5) / 100;
        if (price > offer) {
            offer = RoundOffer(offer);
            stage = Stage::Counter2;
            return HaggleStep{HaggleOutcome::CounterOffer, HaggleMessage::None, offer};
        }
    }
    return AfterCounter2();
}

HaggleStep CarHaggle::AfterCounter2() {
    if (price > offer) {
        stage = Stage::ThirdPrice;
        return HaggleStep{HaggleOutcome::AskPrice, HaggleMessage::None, 0};
    }
    return Settle();
}

HaggleStep CarHaggle::AnswerCounterOffer(bool accepted) {
    if (stage == Stage::Counter1) {
        if (accepted) price = offer;
        return AfterCounter1();
    }
    if (stage == Stage::Counter2) {
        if (accepted) price = offer;
        return AfterCounter2();
    }
    return End(HaggleOutcome::Rejected, HaggleMessage::None);
}

HaggleStep CarHaggle::Settle() {
    // price * 9 and price * 2 are 16-bit multiplies in the original, and the
    // payout is a sign-extended word. Kept: the wrap in the first of them is
    // the $22,001 money cheat, and the sign extension is why a price over
    // 32767 pays out negative.
    const int ninetyPercent = static_cast<uint16_t>(price * 9) / 10;
    if (ninetyPercent > offer) {
        return End(HaggleOutcome::Rejected, HaggleMessage::NoThanks);
    }

    const bool steal = value > static_cast<uint16_t>(price * 2);
    return End(HaggleOutcome::Sold,
               steal ? HaggleMessage::MustBeCrazy : HaggleMessage::IllTakeIt,
               static_cast<int16_t>(price));
}
