// Two dialogs take turns: a price field and a counter-offer. Neither is rebuilt from inside its own button callback.

#include "garage/garage.h"
#include "core/state.h"
#include "core/game_text.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "core/utils.h"

#include <random>

int Garage::CarSaleValue(const CarPtr& car) {
    return state.GetCarValue(car.get());
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
        if (price == 0) {
            if (audioManager) audioManager->PlaySound(AudioSampleType::REJECT_BEEP);   // SR2 1a21:5fb8
            return;
        }

        std::uniform_int_distribution<int> roll5(0, 4);
        HandleHaggleStep(haggle->SubmitPrice(price, roll5(RandomEngine())));
    };
    cfg.onDismiss = [this] { CloseHaggle(); };

    priceEntryDialog = std::make_unique<NumberEntryDialog>(cfg, "I'll take $", fieldRect, 5,
                                                           SDL_Color{0x00, 0x00, 0x00, 0xFF});
    priceEntryDialog->Show();
    SetActiveDialog(priceEntryDialog.get());
}

void Garage::SellSparePart(int row) {
    const std::vector<SpareRef> spares = CollectSpares();
    if (row < 0 || row >= static_cast<int>(spares.size())) return;
    const SpareRef& spare = spares[row];

    std::uniform_int_distribution<int> roll20(0, 19);
    const int price = PartSalePrice(spare.category, spare.value, spare.wear, roll20(RandomEngine()));

    // A worthless part gets its own line rather than an offer of $0.
    const std::string title = price == 0 ? GameText::Msg(GameText::BUYER_FOR_NOTHING) : GameText::Msg(GameText::BUYER_OFFER, price);

    sparePartsDialog->Hide();

    // The "for nothing" line needs the full width.
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
        DialogTextConfig{SDL_Rect{38, 101, 248, 10}, spare.label, {0x00, 0x00, 0x00, 0xFF}, true},
        DialogTextConfig{SDL_Rect{38, 111, 248, 10}, title, {0x00, 0x00, 0x00, 0xFF}, true}
    };
    cfg.buttons = std::vector<ButtonConfig>{
        {yeahRect, "Yeah", nullptr, true, 0},
        {noRect, "No thanks", nullptr, false, 1}
    };
    cfg.onButtonPressed = [this, spare, price](int index) {
        if (index == 0) {
            state.SetBankroll(state.GetBankroll() + price);
            state.AdvanceTicks(600);   // SR1 0x242a6c, SR2 lmain.c 0x2573f0
            state.RemoveSpareAt(spare.category, static_cast<size_t>(spare.index));
            LOG_INFO("Sold spare '%s' for $%d, bankroll now $%d",
                     spare.label.c_str(), price, state.GetBankroll());
        }
        sellPartDialog->Hide();
        SetActiveDialog(nullptr);
        // Rebuilt next frame; doing it here would destroy the dialog whose callback is running.
        sparePartsNeedsReopen = true;
    };
    cfg.onDismiss = [this] {
        SetActiveDialog(nullptr);
        sparePartsNeedsReopen = true;
    };

    sellPartDialog = std::make_unique<Dialog>(cfg);
    sellPartDialog->Show();
    SetActiveDialog(sellPartDialog.get());
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
        DialogTextConfig{SDL_Rect{38, 111, 248, 10}, GameText::Msg(GameText::HAGGLE_COUNTER, amount),
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
            // Rebuilding here would destroy the button whose callback is running.
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

    // SetActiveDialog hides every bubble, so the line goes up after it.
    ShowHaggleMessage(step.message);
}

void Garage::ShowHaggleMessage(HaggleMessage message) {
    if (message == HaggleMessage::None) return;
    if (bubbles.size() <= kSellerBubble) return;

    // The backtick renders as a narrow space.
    GameText::Message id = GameText::HAGGLE_NO_THANKS;
    BubbleStyle style = BUBBLE_RIGHT;
    switch (message) {
        case HaggleMessage::MustBeKidding: id = GameText::HAGGLE_KIDDING; break;
        case HaggleMessage::NoWay:         id = GameText::HAGGLE_NO_WAY; break;
        case HaggleMessage::GetLost:       id = GameText::HAGGLE_GET_LOST; break;
        case HaggleMessage::NoThanks:      id = GameText::HAGGLE_NO_THANKS; break;
        case HaggleMessage::IllTakeIt:     id = GameText::HAGGLE_TAKE_IT; break;
        case HaggleMessage::MustBeCrazy:
            id = GameText::HAGGLE_CRAZY_TAKE_IT;
            style = BUBBLE_LEFT;
            break;
        case HaggleMessage::None:          return;
    }
    const std::string text = GameText::Msg(id);
    bubbles[kSellerBubble]->SetText(text, style);
    bubbles[kSellerBubble]->SetVisible(true);
}

void Garage::FinishCarSale(int price) {
    const bool wasCurrent = (haggleCar == state.GetCurrentCarPtr());

    state.SetBankroll(state.GetBankroll() + price);
    // Four hours, the original's 0x4b0 ticks at 300 per game hour.
    state.AdvanceTicks(1200);   // SR1 0x242f7a, SR2 lmain.c 0x257a86
    state.RemoveOwnedCar(haggleCar);
    // Selling the car you drive leaves you carless even with others parked, as in the original.
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
