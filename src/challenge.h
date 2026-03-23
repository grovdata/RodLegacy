#pragma once

#include "dialogs/dialog.h"
#include "ui/bubble.h"
#include "SFont.h"
#include <memory>
#include <string>
#include <vector>

// Manages the in-diner challenge dialog flow:
//   click car → Race type → Bet → accept (wantsRace) or reject (wantsCarExit)
class Challenge {
public:
    Challenge();

    // Open the dialog flow. Call when the player clicks the opponent car.
    void Open(SFont_Font* font);

    // Forward SDL events. Returns true if the event was consumed.
    bool HandleEvent(SDL_Event* event);

    // Update timers and draw speech bubble + dialog. Call every frame.
    void Draw(float deltaTime);

    // True while the challenge flow is in progress (dialog or speech showing).
    bool IsActive() const;

    // Polled by Diner after Draw(): true for one frame when transition is due.
    bool WantsRace()     const { return wantsRace; }
    bool WantsCarExit()  const { return wantsCarExit; }

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

    static const std::vector<std::string> kInsults;
    static const std::vector<std::string> kAcceptances;

    void ShowRaceTypeDialog();
    void ShowDragBetDialog();
    void ShowRoadBetDialog();
    void ShowSpeech(const std::string& text);
    void Accept();
    void Reject();
};
