#pragma once

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
    int betAttempts = 0;
    // The opponent rejected, so Draw() loops back to the race-type menu once the insult clears.
    bool insultFromOpponent = false;
    // Set by Taunt(): nothing is pending when the insult timer runs out.
    bool tauntOnly = false;

    static const std::vector<std::string> kInsults;
    static const std::vector<std::string> kAcceptances;

    void ShowRaceTypeDialog();
    void ShowDragBetDialog();
    void ShowRoadBetDialog();
    void ShowSpeech(const std::string& text, BubbleStyle style = BUBBLE_RIGHT);
    void Accept();
    void Reject();
    void OpponentRejectsBet();
};
