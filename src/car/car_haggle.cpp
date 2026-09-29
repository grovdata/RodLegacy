#include "car/car_haggle.h"

#include <cstdint>

// The rounded figure is what the rest of the haggle compares against.
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

// Both offers derive from the opening ceiling, which makes refusing the first worthwhile.
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
    // 16-bit multiplies and a sign-extended payout as in the original: the wrap is the $22,001 money cheat.
    const int ninetyPercent = static_cast<uint16_t>(price * 9) / 10;
    if (ninetyPercent > offer) {
        return End(HaggleOutcome::Rejected, HaggleMessage::NoThanks);
    }

    const bool steal = value > static_cast<uint16_t>(price * 2);
    return End(HaggleOutcome::Sold,
               steal ? HaggleMessage::MustBeCrazy : HaggleMessage::IllTakeIt,
               static_cast<int16_t>(price));
}
