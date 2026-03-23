#include "challenge.h"
#include "state.h"
#include "log.h"
#include <cstdlib>

// ---------------------------------------------------------------------------
// String tables
// ---------------------------------------------------------------------------

const std::vector<std::string> Challenge::kInsults = {
    "Blow it out your ear!",
    "Forget it, geek!",
    "Beat it, twerp!",
    "No way, ya nerd!",
    "Get lost!",
    "Give me a break!",
    "Get outta my face!",
    "Buzz off!",
    "Whadda loser!",
    "Chicken!"
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
        cfg.buttons.push_back({{btnX, y, btnW, btnH}, labels[i], nullptr, i == 0, i});
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
    step          = Step::None;
    acceptTimer   = -1.0f;
    insultTimer   = -1.0f;
    wantsRace     = false;
    wantsCarExit  = false;
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

// The move trick: temporarily take ownership so HandleEvent can't destroy
// the dialog while iterating its buttons — callbacks may freely replace `dialog`.
bool Challenge::HandleEvent(SDL_Event* event) {
    if (!dialog || !dialog->IsVisible()) return false;
    auto d = std::move(dialog);
    d->HandleEvent(event);
    if (!dialog) dialog = std::move(d); // restore if callback didn't replace it
    return true;
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
            insultTimer  = -1.0f;
            bubble.reset();
            wantsCarExit = true;
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
            static const int bets[] = {0, 25, 50};
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
            static const int bets[] = {25, 100, 0};
            state.SetPendingRace(RaceType::Road, bets[idx], idx == 2);
            Accept();
        }
    ));
}

void Challenge::ShowSpeech(const std::string& text) {
    bubble = std::make_unique<Bubble>(text, BUBBLE_LEFT, 85, 90, false);
    bubble->SetVisible(true);
}

void Challenge::Accept() {
    step = Step::None;
    dialog.reset();
    ShowSpeech(kAcceptances[rand() % kAcceptances.size()]);
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::Accept - race pending");
}

void Challenge::Reject() {
    step = Step::None;
    dialog.reset();
    ShowSpeech(kInsults[rand() % kInsults.size()]);
    insultTimer = 0.0f;
    LOG_INFO("Challenge::Reject - opponent insulted player");
}
