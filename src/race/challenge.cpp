#include "race/challenge.h"
#include "race/bet_acceptance.h"
#include "core/state.h"
#include "core/game_text.h"
#include "core/log.h"
#include "core/utils.h"
#include <cstdlib>
#include <random>

namespace {
// rand() is never seeded, so it would replay the same sequence every launch.
int Roll(int n) {
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(RandomEngine());
}
}

const std::vector<GameText::Message> Challenge::kInsults = {
    GameText::INSULT_1, GameText::INSULT_2, GameText::INSULT_3, GameText::INSULT_4, GameText::INSULT_5,
    GameText::CHICKEN};
const std::vector<GameText::Message> Challenge::kAcceptances = {
    GameText::ACCEPT_1, GameText::ACCEPT_2, GameText::ACCEPT_3,
    GameText::DRAG_ACCEPT_1, GameText::DRAG_ACCEPT_2, GameText::DRAG_ACCEPT_3};

static Dialog* MakeDialog(const std::vector<std::string>& labels,
                           SFont_Font* font,
                           std::function<void(int)> onPressed)
{
    const int dlgX = 37;
    const int dlgW = 84;
    const int btnX = dlgX;
    const int btnW = dlgW;
    const int btnH = 16;
    const int topPad = 0, spacing = 0;
    int dlgH = topPad + static_cast<int>(labels.size()) * (btnH + spacing) - spacing + topPad;
    int dlgY = (200 - dlgH) / 2;

    DialogConfig cfg;
    cfg.font   = font;
    cfg.bounds = {dlgX, dlgY, dlgW, dlgH};
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor     = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.onButtonPressed = onPressed;

    int y = dlgY + topPad;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        ButtonConfig btn = {{btnX, y, btnW, btnH}, labels[i], nullptr, i == 0, i};
        // Every label list ends with "Forget it", so Escape backs out the same way.
        btn.cancelChoice = (i == static_cast<int>(labels.size()) - 1);
        cfg.buttons.push_back(btn);
        y += btnH + spacing;
    }

    auto* dlg = new Dialog(cfg);
    dlg->Show();
    return dlg;
}

Challenge::Challenge() = default;

void Challenge::Reset() {
    dialog.reset();
    bubble.reset();
    step             = Step::None;
    acceptTimer      = -1.0f;
    insultTimer      = -1.0f;
    wantsRace        = false;
    wantsCarExit     = false;
    betAttempts      = 0;
    insultFromOpponent = false;
    tauntOnly          = false;
}

void Challenge::Taunt() {
    if (IsActive()) return;
    step = Step::None;
    dialog.reset();
    insultFromOpponent = false;
    tauntOnly = true;
    ShowSpeech(GameText::Msg(kInsults[Roll(static_cast<int>(kInsults.size()))]));
    insultTimer = 0.0f;
    LOG_INFO("Challenge::Taunt");
}

bool Challenge::IsActive() const {
    return step != Step::None
        || (dialog && dialog->IsVisible())
        || bubble
        || acceptTimer >= 0.0f
        || insultTimer >= 0.0f;
}

void Challenge::Open(SFont_Font* f) {
    if (IsActive()) return;
    font = f;
    ShowRaceTypeDialog();
}

void Challenge::OpenKing(SFont_Font* f) {
    if (IsActive()) return;
    font = f;

    // Before he is unlocked, one of four lines by reputation tier; tier 1 is silent.
    const int tier = state.GetReputation().GetOpponentTier(state.GetRacerPool().CountActive());
    if (tier != 0) {
        static const GameText::Message kTierLines[] = {
            GameText::KING_AMATEUR, GameText::KING_WIN_MORE, GameText::KING_MORE_DRAG, GameText::KING_MORE_ROAD};
        if (tier >= 2 && tier <= 5) {
            step = Step::None;
            ShowSpeech(GameText::Msg(kTierLines[tier - 2]), tier == 3 ? BUBBLE_RIGHT_TWO_LINES : BUBBLE_LEFT);
            insultTimer = 0.0f;
            insultFromOpponent = false;
        }
        LOG_INFO("Challenge::OpenKing - not yet unlocked, tier=%d", tier);
        return;
    }

    const bool qualified = state.IsKingQualifierWon();
    if (!qualified) {
        Car* car = state.GetCurrentCar();
        if (!car || car->GetFuelTenths() < 22) {
            step = Step::None;
            ShowSpeech(GameText::Msg(GameText::KING_NEED_GAS), BUBBLE_RIGHT_TWO_LINES);
            insultTimer = 0.0f;
            insultFromOpponent = false;
            LOG_INFO("Challenge::OpenKing - refused, not enough fuel");
            return;
        }
    }

    state.SetPendingRace(qualified ? RaceType::Road : RaceType::Drag, 0, qualified);
    // Accept() would pick a random acceptance line over the King's own.
    step = Step::None;
    dialog.reset();
    if (qualified) {
        ShowSpeech(GameText::Msg(GameText::KING_PINK_SLIP_ON), BUBBLE_RIGHT_TWO_LINES);
    } else {
        ShowSpeech(GameText::Msg(GameText::KING_DRAG_ON), BUBBLE_LEFT);
    }
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::OpenKing - race pending, qualified=%d", qualified);
}

// Take ownership so callbacks can replace `dialog` while its buttons are iterated.
bool Challenge::HandleEvent(SDL_Event* event) {
    if (!dialog || !dialog->IsVisible()) return false;
    auto d = std::move(dialog);
    bool handled = d->HandleEvent(event);
    // Restore only if the callback didn't end or replace the flow.
    // Accept/Reject set step=None; in that case the dialog stays dismissed.
    if (!dialog && step != Step::None) dialog = std::move(d);
    return handled;
}

void Challenge::Draw(float deltaTime) {
    if (bubble)
        bubble->Draw();

    if (dialog && dialog->IsVisible())
        dialog->Draw();

    if (acceptTimer >= 0.0f) {
        acceptTimer += deltaTime;
        if (acceptTimer >= 1.5f) {
            acceptTimer = -1.0f;
            wantsRace   = true;
        }
    }

    if (insultTimer >= 0.0f) {
        insultTimer += deltaTime;
        if (insultTimer >= 2.0f) {
            insultTimer = -1.0f;
            bubble.reset();
            if (tauntOnly) { tauntOnly = false; return; }
            // The opponent gets a second look at a bet; the player's own "Forget it" ends things on the spot.
            if (insultFromOpponent && betAttempts < kMaxBetAttempts) {
                ShowRaceTypeDialog();
            } else {
                wantsCarExit = true;
            }
        }
    }
}

void Challenge::ShowRaceTypeDialog() {
    step = Step::RaceType;
    dialog.reset(MakeDialog(
        {"Road Race", "Drag Race", "Forget it"},
        font,
        [this](int idx) {
            switch (idx) {
                case 0: ShowRoadBetDialog(); break;
                case 1: ShowDragBetDialog(); break;
                case 2: Reject();            break;
            }
        }
    ));
}

void Challenge::ShowDragBetDialog() {
    step = Step::DragBet;
    dialog.reset(MakeDialog(
        {"Just for kicks", "$25", "$50", "Forget it"},
        font,
        [this](int idx) {
            if (idx == 3) { Reject(); return; }

            static const BetType betTypes[] = {
                BetType::NoMoney, BetType::SmallMoney, BetType::LargeMoney
            };
            static const int bets[] = {0, 25, 50};

            BetType bet = betTypes[idx];
            int roll    = Roll(100);
            bool accepted = OpponentAcceptsBet(state.GetBetContext(), bet, roll);

            if (!accepted) { OpponentRejectsBet(); return; }

            state.SetPendingRace(RaceType::Drag, bets[idx], false);
            Accept();
        }
    ));
}

void Challenge::ShowRoadBetDialog() {
    step = Step::RoadBet;
    dialog.reset(MakeDialog(
        {"$25", "$100", "Pink slips", "Forget it"},
        font,
        [this](int idx) {
            if (idx == 3) { Reject(); return; }

            static const BetType betTypes[] = {
                BetType::SmallMoney, BetType::LargeMoney, BetType::PinkSlips
            };
            static const int bets[] = {25, 100, 0};

            BetType bet = betTypes[idx];
            int roll    = Roll(100);
            bool accepted = OpponentAcceptsBet(state.GetBetContext(), bet, roll);

            if (!accepted) { OpponentRejectsBet(); return; }

            state.SetPendingRace(RaceType::Road, bets[idx], idx == 2);
            Accept();
        }
    ));
}

void Challenge::ShowSpeech(const std::string& text, BubbleStyle style) {
    bubble = std::make_unique<Bubble>(text, style, false);
    bubble->SetVisible(true);
}

void Challenge::Accept() {
    step = Step::None;
    dialog.reset();
    ShowSpeech(GameText::Msg(kAcceptances[Roll(static_cast<int>(kAcceptances.size()))]));
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::Accept - race pending");
}

void Challenge::Reject() {
    step = Step::None;
    dialog.reset();
    insultFromOpponent = false;
    ShowSpeech(GameText::Msg(kInsults[Roll(static_cast<int>(kInsults.size()))]));
    insultTimer = 0.0f;
    LOG_INFO("Challenge::Reject - player backed out");
}

// Up to kMaxBetAttempts tries before the encounter ends.
void Challenge::OpponentRejectsBet() {
    step = Step::None;
    dialog.reset();
    ++betAttempts;
    insultFromOpponent = true;
    ShowSpeech(GameText::Msg(kInsults[Roll(static_cast<int>(kInsults.size()))]));
    insultTimer = 0.0f;
    LOG_INFO("Challenge::OpponentRejectsBet - attempt %d/%d", betAttempts, kMaxBetAttempts);
}
