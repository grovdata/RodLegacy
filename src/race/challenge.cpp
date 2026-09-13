#include "race/challenge.h"
#include "race/bet_acceptance.h"
#include "core/state.h"
#include "core/log.h"
#include "core/utils.h"
#include <cstdlib>
#include <random>

namespace {
// rand() is never seeded anywhere in RodLegacy, so it replays the same
// sequence on every launch - which made a bet's outcome a fixed function of
// how many draws had happened, identical in every new game. RandomEngine()
// is the seeded one the rest of the code already uses.
int Roll(int n) {
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(RandomEngine());
}
}  // namespace

// ---------------------------------------------------------------------------
// String tables
// ---------------------------------------------------------------------------

const std::vector<std::string> Challenge::kInsults = {
    "  Get lost!",
    "  Give me a break!",
    "  Get outta my face!",
    "  Buzz off!",
    "  Whadda loser!",
    "  Chicken!"
};

const std::vector<std::string> Challenge::kAcceptances = {
    "You're on!",
    "Alright!",
    "Let's go!",
    "Cool!",
    "O.K.!",
    "Let's hit it!"
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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
        // Every one of these dialogs ends its label list with "Forget it" -
        // Escape should back out the same way clicking it does.
        btn.cancelChoice = (i == static_cast<int>(labels.size()) - 1);
        cfg.buttons.push_back(btn);
        y += btnH + spacing;
    }

    auto* dlg = new Dialog(cfg);
    dlg->Show();
    return dlg;
}

// ---------------------------------------------------------------------------
// Challenge
// ---------------------------------------------------------------------------

Challenge::Challenge() = default;

void Challenge::Reset() {
    dialog.reset();
    bubble.reset();
    step             = Step::None;
    acceptTimer      = -1.0f;
    insultTimer      = -1.0f;
    wantsRace        = false;
    wantsCarExit     = false;
    wasOpponentRefusal = false;
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
    ShowSpeech(kInsults[Roll(static_cast<int>(kInsults.size()))]);
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

// All lines (messages 62/63/64) confirmed from the disassembly's string table.
void Challenge::OpenKing(SFont_Font* f) {
    if (IsActive()) return;
    font = f;

    // Ported from RaceSetupLoop's GetOpponentDifficulty() dispatch
    // (Amiga 0x0023d03a / 0x0023d8e2-0x0023d92a): before he's unlocked, a
    // click on The King is rebuffed with one of four lines picked by
    // reputation tier - tier 1 (total rep < 3) is silent, not even a bubble.
    const int tier = state.GetReputation().GetOpponentTier(state.GetRacerPool().CountActive());
    if (tier != 0) {
        const char* line = nullptr;
        switch (tier) {
            case 2: line = "  I don't got time for amateurs like you!"; break;
            case 3: line = "  You're doing OK kid, but you're gonna need to win more races!"; break;
            case 4: line = "  You're gonna hafta win more drag races!"; break;
            case 5: line = "  You're gonna hafta win more road races!"; break;
            default: break;  // tier 1: silent
        }
        if (line) {
            step = Step::None;
            ShowSpeech(line);
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
            ShowSpeech("  You're gonna hafta get more gas before you race.");
            insultTimer = 0.0f;
            insultFromOpponent = false;
            LOG_INFO("Challenge::OpenKing - refused, not enough fuel");
            return;
        }
    }

    state.SetPendingRace(qualified ? RaceType::Road : RaceType::Drag, 0, qualified);
    // Inline the tail of Accept(): it always picks a random line from
    // kAcceptances, which would stomp the King-specific one below.
    step = Step::None;
    dialog.reset();
    // Confirmed against RaceSetupLoop (Amiga 0x0023d954/0x0023d96e) and its
    // string table (messages 0x3e/0x3f, LAB_153F and LAB_1540+LAB_1541).
    ShowSpeech(qualified ? "  Cool, dude! Get your pink slip out, and let's go!"
                         : "  You're on! Let's hit the drag race!");
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::OpenKing - race pending, qualified=%d", qualified);
}

// The move trick: temporarily take ownership so HandleEvent can't destroy
// the dialog while iterating its buttons — callbacks may freely replace `dialog`.
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
            // The opponent gets a second look at a bet before the player is
            // sent packing (RaceSetupLoop's 2-attempt loop); the player's own
            // "Forget it" always ends things on the spot.
            if (insultFromOpponent && betAttempts < kMaxBetAttempts) {
                ShowRaceTypeDialog();
            } else {
                wasOpponentRefusal = insultFromOpponent;
                wantsCarExit = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

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

void Challenge::ShowSpeech(const std::string& text) {
    // TODO: Should adjust depending on car model
    bubble = std::make_unique<Bubble>(text, BUBBLE_RIGHT, 85, 70, false);
    bubble->SetVisible(true);
}

void Challenge::Accept() {
    step = Step::None;
    dialog.reset();
    ShowSpeech(kAcceptances[Roll(static_cast<int>(kAcceptances.size()))]);
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::Accept - race pending");
}

void Challenge::Reject() {
    step = Step::None;
    dialog.reset();
    insultFromOpponent = false;
    ShowSpeech(kInsults[Roll(static_cast<int>(kInsults.size()))]);
    insultTimer = 0.0f;
    LOG_INFO("Challenge::Reject - player backed out");
}

// Opponent turned down a proposed bet. Ported from RaceSetupLoop (Amiga
// 0x0023d8c8): up to kMaxBetAttempts tries before the encounter ends, not
// just one - see RodLegacyResearch/docs/diner_challenge.md.
void Challenge::OpponentRejectsBet() {
    step = Step::None;
    dialog.reset();
    ++betAttempts;
    insultFromOpponent = true;
    ShowSpeech(kInsults[Roll(static_cast<int>(kInsults.size()))]);
    insultTimer = 0.0f;
    LOG_INFO("Challenge::OpponentRejectsBet - attempt %d/%d", betAttempts, kMaxBetAttempts);
}
