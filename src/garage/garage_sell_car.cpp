// Selling a car from the garage's "Your cars" list. The negotiation is
// CarHaggle (car_haggle.h); everything here is the dialogs it drives.
//
// Two dialogs take turns: a price field ("I'll take $____", Offer / Never
// mind) and a counter-offer ("How about $N ?", Yeah / No thanks). Neither is
// ever rebuilt from inside its own button callback - the price dialog is
// reused across prompts and only cleared, and a counter-offer can never
// directly follow a counter-offer.

#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "utils.h"

#include <random>

// The bubble the haggle talks through. Appended last in Garage's constructor.
static constexpr size_t kHaggleBubble = 14;

int Garage::CarSaleValue(const CarPtr& car) {
    if (!car) return 0;
    // ponytail: the newspaper list price, not a running total that the garage
    // adjusts as parts go on and off - which is what the original keeps on the
    // car record. Fitting or pulling parts does not move what a car fetches
    // here yet.
    return state.carData.GetCarMetadata(car->GetModel().index).classified.price;
}

void Garage::StartSellCar(const CarPtr& car) {
    const int value = CarSaleValue(car);
    if (value <= 0) {
        LOG_INFO("Garage::StartSellCar: no list price known for this car, not selling");
        SetActiveDialog(nullptr);
        return;
    }

    haggleCar = car;
    std::uniform_int_distribution<int> roll20(0, 19);
    haggle = std::make_unique<CarHaggle>(value, roll20(RandomEngine()));
    LOG_INFO("Selling %s: worth $%d, buyer will go to $%d",
             car->GetModel().name.c_str(), value, haggle->BuyerCeiling());

    SDL_Rect dialogRect = {34, 97, 252, 46};
    SDL_Rect offerRect = {88, 123, 66, 16};
    SDL_Rect neverMindRect = {166, 123, 66, 16};
    SDL_Rect fieldRect = {38, 111, 248, 10};

    DialogConfig cfg;
    cfg.bounds = dialogRect;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{dialogRect};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = std::vector<DialogTextConfig>{
        DialogTextConfig{SDL_Rect{38, 101, 248, 10}, car->GetModel().name, {0x00, 0x00, 0x00, 0xFF}, true}
    };
    cfg.buttons = std::vector<ButtonConfig>{
        {offerRect, "Offer", nullptr, true, 0},
        {neverMindRect, "Never mind", nullptr, false, 1}
    };
    cfg.onButtonPressed = [this](int index) {
        if (index != 0) {
            CloseHaggle();
            return;
        }
        // An empty field is not an offer; the original keeps asking.
        const int price = priceEntryDialog->Value();
        if (price == 0) return;

        std::uniform_int_distribution<int> roll5(0, 4);
        HandleHaggleStep(haggle->SubmitPrice(price, roll5(RandomEngine())));
    };
    cfg.onDismiss = [this] { CloseHaggle(); };

    priceEntryDialog = std::make_unique<NumberEntryDialog>(cfg, "I'll take $", fieldRect, 5,
                                                           SDL_Color{0x00, 0x00, 0x00, 0xFF});
    priceEntryDialog->Show();
    SetActiveDialog(priceEntryDialog.get());
}

void Garage::ShowCounterOfferDialog(int amount) {
    priceEntryDialog->Hide();

    SDL_Rect dialogRect = {34, 97, 252, 46};
    SDL_Rect yeahRect = {88, 123, 66, 16};
    SDL_Rect noRect = {166, 123, 66, 16};

    DialogConfig cfg;
    cfg.bounds = dialogRect;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{dialogRect};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = std::vector<DialogTextConfig>{
        DialogTextConfig{SDL_Rect{38, 101, 248, 10}, haggleCar->GetModel().name, {0x00, 0x00, 0x00, 0xFF}, true},
        DialogTextConfig{SDL_Rect{38, 111, 248, 10}, "How about $" + std::to_string(amount) + " ?",
                         {0x00, 0x00, 0x00, 0xFF}, true}
    };
    cfg.buttons = std::vector<ButtonConfig>{
        {yeahRect, "Yeah", nullptr, true, 0},
        {noRect, "No thanks", nullptr, false, 1}
    };
    cfg.onButtonPressed = [this](int index) {
        counterOfferDialog->Hide();
        HandleHaggleStep(haggle->AnswerCounterOffer(index == 0));
    };
    cfg.onDismiss = [this] { CloseHaggle(); };

    counterOfferDialog = std::make_unique<Dialog>(cfg);
    counterOfferDialog->Show();
    SetActiveDialog(counterOfferDialog.get());
}

void Garage::HandleHaggleStep(const HaggleStep& step) {
    switch (step.outcome) {
        case HaggleOutcome::AskPrice:
            // Same dialog, empty field - rebuilding it here would destroy the
            // button whose callback is running.
            priceEntryDialog->ClearEntry();
            priceEntryDialog->Show();
            SetActiveDialog(priceEntryDialog.get());
            break;
        case HaggleOutcome::CounterOffer:
            ShowCounterOfferDialog(step.amount);
            break;
        case HaggleOutcome::Sold:
            // Before CloseHaggle, which lets go of the car being sold.
            FinishCarSale(step.amount);
            CloseHaggle();
            break;
        case HaggleOutcome::Rejected:
            CloseHaggle();
            break;
    }

    // Last: Scene::SetActiveDialog hides every bubble as it opens a dialog,
    // so the buyer's line has to go up after that, not before.
    ShowHaggleMessage(step.message);
}

void Garage::ShowHaggleMessage(HaggleMessage message) {
    if (message == HaggleMessage::None) return;
    if (bubbles.size() <= kHaggleBubble) return;

    // Original strings; the backtick renders as a space (see the bubbles in
    // Garage's constructor).
    const char* text = "";
    switch (message) {
        case HaggleMessage::MustBeKidding: text = "  You must be kidding`!"; break;
        case HaggleMessage::NoWay:         text = "  No way`!"; break;
        case HaggleMessage::GetLost:       text = "  Get lost`!"; break;
        case HaggleMessage::NoThanks:      text = "  No thanks`!"; break;
        case HaggleMessage::IllTakeIt:     text = "  I'll take it`!"; break;
        case HaggleMessage::MustBeCrazy:   text = "  You must be crazy. I'll take it`!"; break;
        case HaggleMessage::None:          return;
    }
    bubbles[kHaggleBubble]->SetText(text);
    bubbles[kHaggleBubble]->SetVisible(true);
}

void Garage::FinishCarSale(int price) {
    const bool wasCurrent = (haggleCar == state.GetCurrentCarPtr());

    state.SetBankroll(state.GetBankroll() + price);
    // Four hours, the original's 0x4b0 ticks at 300 per game hour.
    state.AdvanceGameHours(4);
    state.RemoveOwnedCar(haggleCar);
    // Selling the car you are driving leaves you carless even if others are
    // parked here, which is what the original does.
    if (wasCurrent) {
        state.SetCurrentCar(nullptr);
    }

    LOG_INFO("Sold %s for $%d, bankroll now $%d, %zu cars left",
             haggleCar->GetModel().name.c_str(), price, state.GetBankroll(),
             state.GetOwnedCars().size());

    haggleCar.reset();
    // ponytail: the car vanishes rather than driving off the way Switch it
    // animates one out. Add the animation if the pop looks wrong on screen.
    DropUnownedCarBitmaps();
    UpdateSign();
}

void Garage::CloseHaggle() {
    if (priceEntryDialog) priceEntryDialog->Hide();
    if (counterOfferDialog) counterOfferDialog->Hide();
    SetActiveDialog(nullptr);
    haggle.reset();
    haggleCar.reset();
}
