/*
 * RodLegacy - The diner challenge flow: Challenge runs the race-type and bet dialogs and the opponent's
 * replies.
 *
 * Created: 2026-03-23
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/game_text.h"
#include "race/reputation.h"
#include "dialogs/dialog.h"
#include "ui/bubble.h"
#include "gfx/SFont.h"
#include <memory>
#include <string>
#include <vector>

class Challenge {
public:
    Challenge();

    void Open(SFont_Font* font);

    // The King: forced drag (free) then forced road (pink slip), no bet dialog or accept roll.
    void OpenKing(SFont_Font* font);
    void OpenKingSr2();

    bool HandleEvent(SDL_Event* event);

    void Draw(float deltaTime);

    bool IsActive() const;

    bool WantsRace()     const { return wantsRace; }
    bool WantsCarExit()  const { return wantsCarExit; }

    // A taunt with no encounter attached, used when the player pops the hood a second time.
    void Taunt();

    void Reset();

private:
    enum class Step { None, RaceType, DragBet, RoadBet };

    SFont_Font* font = nullptr;
    std::unique_ptr<Dialog> dialog;
    std::unique_ptr<Bubble> bubble;
    Step step = Step::None;

    float acceptTimer = -1.0f;
    float insultTimer = -1.0f;
    bool wantsRace     = false;
    bool wantsCarExit  = false;

    // The opponent may reject a proposed bet twice before the encounter ends.
    static constexpr int kMaxBetAttempts = 2;
    int lastRefusal = -1;
    int betAttempts = 0;
    // The opponent rejected, so Draw() loops back to the race-type menu once the insult clears.
    bool insultFromOpponent = false;
    // Set by Taunt(): nothing is pending when the insult timer runs out.
    bool tauntOnly = false;

    static const std::vector<GameText::Message> kInsults;

    void ShowRaceTypeDialog();
    void ShowDragBetDialog();
    void ShowRoadBetDialog(RaceType type);
    // Street Rod 2 refuses a race the car has not the fuel for. Returns true
    // when it refused, and the challenge is over.
    bool RefuseForGas(RaceType type);
    void ShowSpeech(const std::string& text, BubbleStyle style = BUBBLE_RIGHT);
    void Accept(bool drag = false);
    void Reject();
    void OpponentRejectsBet(bool drag = false);
    void NotEnoughDough();
};
