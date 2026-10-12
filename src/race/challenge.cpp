#include "race/race_types.h"
#include "race/challenge.h"
#include "race/bet_acceptance.h"
#include "core/state.h"
#include "core/log.h"
#include "core/utils.h"
#include <algorithm>
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

static Dialog* MakeDialog(const std::vector<std::string>& labels,
                           SFont_Font* font,
                           std::function<void(int)> onPressed,
                           int dlgX = 37, int dlgW = 84)
{
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
    lastRefusal      = -1;
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
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        OpenKingSr2();
        return;
    }

    // Before he is unlocked, one of four lines by reputation tier; tier 1 is silent.
    const int tier = state.GetReputation().GetOpponentTier(state.GetRacerPool().CountActive());
    if (tier != 0) {
        // Tier 1 is silent.
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

// Street Rod 2's King (ldrivein.c FUN_00250598, docs/sr2_amiga/king.md): refused by FUN_0024fc16's tier, else
// three races in turn - a drag race, Mulholland, then the Aqueduct for pink slips.
void Challenge::OpenKingSr2() {
    auto say = [&](GameText::Message line, BubbleStyle style) {
        step = Step::None;
        dialog.reset();
        ShowSpeech(GameText::Msg(line), style);
        insultTimer = 0.0f;
        insultFromOpponent = false;
    };

    const int tier = state.GetReputation().GetOpponentTier(state.GetRacerPool().CountActive(), true);
    if (tier != 0) {
        switch (tier) {
            case 2: say(GameText::KING_AMATEUR, BUBBLE_LEFT); break;
            case 3: say(GameText::KING_WIN_MORE, BUBBLE_RIGHT_TWO_LINES); break;
            case 4: say(GameText::KING_NEED_GRUDGE, BUBBLE_RIGHT_TWO_LINES); break;
            case 5: say(GameText::KING_MORE_MULHOLLAND, BUBBLE_RIGHT_TWO_LINES); break;
            case 6: say(GameText::KING_MORE_AQUEDUCT, BUBBLE_RIGHT_TWO_LINES); break;
            default: break;  // tier 1: silent
        }
        LOG_INFO("Challenge::OpenKingSr2 - not yet, tier=%d", tier);
        return;
    }

    const int stage = std::clamp(state.GetReputation().king_stage, 0, 2);
    static constexpr RaceType kTypes[3] = {RaceType::Drag, RaceType::Road, RaceType::Aqueduct};
    static constexpr srrace::RaceKind kKinds[3] = {srrace::RaceKind::Drag, srrace::RaceKind::Mulholland,
                                                   srrace::RaceKind::Aqueduct};
    const Car* car = state.GetCurrentCar();
    if (!car || car->GetFuelTenths() < srrace::RaceMinimumGas(kKinds[stage])) {
        say(GameText::KING_NEED_GAS, BUBBLE_RIGHT_TWO_LINES);
        LOG_INFO("Challenge::OpenKingSr2 - refused, not enough fuel for stage %d", stage);
        return;
    }

    // Nothing at stake until the Aqueduct, which is for the cars.
    state.SetPendingRace(kTypes[stage], 0, stage == 2);
    switch (stage) {
        case 0: say(GameText::KING_DRAG_ON, BUBBLE_LEFT); break;
        case 1: say(GameText::KING_MULHOLLAND_ON, BUBBLE_LEFT); break;
        default: say(GameText::KING_PINK_SLIP_ON, BUBBLE_RIGHT_TWO_LINES); break;
    }
    insultTimer = -1.0f;
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::OpenKingSr2 - race pending, stage=%d", stage);
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


// ldrivein.c FUN_00250598 checks the tank before the wager dialog.
bool Challenge::RefuseForGas(RaceType type) {
    const Car* car = state.GetCurrentCar();
    if (!car) {
        return false;
    }
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    srrace::RaceKind kind = srrace::RaceKind::Road;
    if (type == RaceType::Drag) {
        kind = srrace::RaceKind::Drag;
    } else if (type == RaceType::Aqueduct) {
        kind = srrace::RaceKind::Aqueduct;
    } else if (sr2) {
        kind = srrace::RaceKind::Mulholland;
    }

    const int need = srrace::RaceMinimumGas(kind);
    if (need == 0 || car->GetFuelTenths() >= need) {
        return false;
    }

    step = Step::None;
    dialog.reset();
    insultFromOpponent = false;
    ShowSpeech(GameText::Msg(GameText::KING_NEED_GAS), BUBBLE_RIGHT_TWO_LINES);
    insultTimer = 0.0f;
    return true;
}

void Challenge::ShowRaceTypeDialog() {
    step = Step::RaceType;
    // Street Rod 2 runs its road race on Mulholland and adds the Aqueduct;
    // Street Rod has neither name and just the two races.
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    std::vector<std::string> options = sr2
        ? std::vector<std::string>{"Drag race", "Mulholland drive", "Aqueduct race", "Forget it"}
        : std::vector<std::string>{"Road Race", "Drag Race", "Forget it"};

    dialog.reset(MakeDialog(
        options,
        font,
        [this, sr2](int idx) {
            if (!sr2) {
                switch (idx) {
                    case 0:
                        if (!RefuseForGas(RaceType::Road)) ShowRoadBetDialog(RaceType::Road);
                        break;
                    case 1:
                        if (!RefuseForGas(RaceType::Drag)) ShowDragBetDialog();
                        break;
                    default: Reject(); break;
                }
                return;
            }
            // Dialog 15's order: drag, Mulholland, Aqueduct.
            switch (idx) {
                case 0:
                    if (!RefuseForGas(RaceType::Drag)) ShowDragBetDialog();
                    break;
                case 1:
                    if (!RefuseForGas(RaceType::Road)) ShowRoadBetDialog(RaceType::Road);
                    break;
                case 2:
                    if (!RefuseForGas(RaceType::Aqueduct)) {
                        ShowRoadBetDialog(RaceType::Aqueduct);
                    }
                    break;
                default: Reject(); break;
            }
        },
        // SR2's dialog 15 spans x 47..141 so "Mulholland drive" fits.
        sr2 ? 47 : 37, sr2 ? 95 : 84
    ));
}

void Challenge::ShowDragBetDialog() {
    step = Step::DragBet;
    // Street Rod 2's dialog 13 offers $10 where Street Rod offers $25.
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    dialog.reset(MakeDialog(
        {"Just for kicks", sr2 ? "$10" : "$25", "$50", "Forget it"},
        font,
        [this, sr2](int idx) {
            if (idx == 3) { Reject(); return; }

            static const BetType betTypes[] = {
                BetType::NoMoney, BetType::SmallMoney, BetType::LargeMoney
            };
            const int bets[] = {0, sr2 ? 10 : 25, 50};
            if (sr2 && bets[idx] > state.GetBankroll()) { NotEnoughDough(); return; }

            BetType bet = betTypes[idx];
            int roll    = Roll(100);
            bool accepted = OpponentAcceptsBet(state.GetBetContext(), bet, roll);

            if (!accepted) { OpponentRejectsBet(true); return; }

            state.SetPendingRace(RaceType::Drag, bets[idx], false);
            Accept(true);
        },
        // SR2's dialog 13 spans x 47..127.
        sr2 ? 47 : 37, sr2 ? 81 : 84
    ));
}

void Challenge::ShowRoadBetDialog(RaceType type) {
    step = Step::RoadBet;
    // Street Rod 2's dialog 14 offers $150 where Street Rod offers $100.
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    dialog.reset(MakeDialog(
        {"$25", sr2 ? "$150" : "$100", "Pink slips", "Forget it"},
        font,
        [this, type, sr2](int idx) {
            if (idx == 3) { Reject(); return; }

            static const BetType betTypes[] = {
                BetType::SmallMoney, BetType::LargeMoney, BetType::PinkSlips
            };
            const int bets[] = {25, sr2 ? 150 : 100, 0};
            if (sr2 && bets[idx] > state.GetBankroll()) { NotEnoughDough(); return; }

            BetType bet = betTypes[idx];
            int roll    = Roll(100);
            bool accepted = OpponentAcceptsBet(state.GetBetContext(), bet, roll);

            if (!accepted) { OpponentRejectsBet(); return; }

            state.SetPendingRace(type, bets[idx], idx == 2);
            Accept();
        },
        // SR2's dialog 14 spans x 63..125.
        sr2 ? 63 : 37, sr2 ? 63 : 84
    ));
}

void Challenge::ShowSpeech(const std::string& text, BubbleStyle style) {
    bubble = std::make_unique<Bubble>(text, style, false);
    bubble->SetVisible(true);
}

void Challenge::Accept(bool drag) {
    step = Step::None;
    dialog.reset();
    // Three lines a race, the drag race's own and the road race's (SR2 ldrivein.c 0x2508fa, table A4-0x1f6c; SR1 has
    // the same table at file 0x2c5cc).
    static const GameText::Message kDrag[3] = {GameText::DRAG_ACCEPT_1, GameText::DRAG_ACCEPT_2, GameText::DRAG_ACCEPT_3};
    static const GameText::Message kRoad[3] = {GameText::ACCEPT_1, GameText::ACCEPT_2, GameText::ACCEPT_3};
    ShowSpeech(GameText::Msg((drag ? kDrag : kRoad)[Roll(3)]));
    acceptTimer = 0.0f;
    LOG_INFO("Challenge::Accept - race pending");
}

void Challenge::Reject() {
    step = Step::None;
    dialog.reset();
    insultFromOpponent = false;
    // Street Rod 2 has one word for backing out (ldrivein.c 0x25082c).
    ShowSpeech(GameText::Msg(state.GetAssets().GetGame() == SrGame::SR2
                                  ? GameText::CHICKEN : kInsults[Roll(static_cast<int>(kInsults.size()))]));
    insultTimer = 0.0f;
    LOG_INFO("Challenge::Reject - player backed out");
}

// Street Rod 2 checks the stake against the bankroll before asking (ldrivein.c 0x2508aa); it uses up a try.
void Challenge::NotEnoughDough() {
    step = Step::None;
    dialog.reset();
    ++betAttempts;
    insultFromOpponent = true;
    ShowSpeech(GameText::Msg(GameText::BET_TOO_BIG));
    insultTimer = 0.0f;
}

// Up to kMaxBetAttempts tries before the encounter ends.
void Challenge::OpponentRejectsBet(bool drag) {
    step = Step::None;
    dialog.reset();
    ++betAttempts;
    insultFromOpponent = true;
    // Five a race, never the same one twice running (SR2 ldrivein.c 0x25095a, table A4-0x1f80; SR1 file 0x2c5b8).
    static const GameText::Message kDrag[5] = {GameText::DRAG_REFUSE_1, GameText::DRAG_REFUSE_2,
                                               GameText::DRAG_REFUSE_3, GameText::DRAG_REFUSE_4, GameText::INSULT_1};
    static const GameText::Message kRoad[5] = {GameText::INSULT_1, GameText::INSULT_2, GameText::INSULT_3,
                                               GameText::INSULT_4, GameText::INSULT_5};
    int pick = Roll(5);
    while (pick == lastRefusal) pick = Roll(5);
    lastRefusal = pick;
    ShowSpeech(GameText::Msg((drag ? kDrag : kRoad)[pick]));
    insultTimer = 0.0f;
    LOG_INFO("Challenge::OpponentRejectsBet - attempt %d/%d", betAttempts, kMaxBetAttempts);
}
