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

    // The King: no race-type/bet dialog, no accept/reject roll - forced drag
    // (free) then forced road (pink slip), gated on fuel >= 22 tenths for the
    // first challenge only. See king.md.
    void OpenKing(SFont_Font* font);

    // Forward SDL events. Returns true if the event was consumed.
    bool HandleEvent(SDL_Event* event);

    // Update timers and draw speech bubble + dialog. Call every frame.
    void Draw(float deltaTime);

    // True while the challenge flow is in progress (dialog or speech showing).
    bool IsActive() const;

    // Polled by Diner after Draw(): true for one frame when transition is due.
    bool WantsRace()     const { return wantsRace; }
    bool WantsCarExit()  const { return wantsCarExit; }
    // True when WantsCarExit() fired because the OPPONENT turned down a bet
    // (as opposed to the player picking "Forget it"). Diner uses this to
    // roll the King's 1% cameo chance on the next opponent - see
    // State::RequestKingCameoRoll.
    bool WasOpponentRefusal() const { return wasOpponentRefusal; }

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
    bool wasOpponentRefusal = false;

    // Ported from RaceSetupLoop (Amiga 0x0023d8c8): the opponent gets to
    // reject a proposed bet twice before the encounter ends, not once. Counts
    // opponent rejections only - the player choosing "Forget it" ends things
    // immediately regardless of this counter. See
    // RodLegacyResearch/docs/diner_challenge.md.
    static constexpr int kMaxBetAttempts = 2;
    int betAttempts = 0;
    // True while insultTimer is running because the OPPONENT rejected a bet
    // (as opposed to the player picking "Forget it"), so Draw() knows whether
    // to loop back to the race-type menu once the insult clears.
    bool insultFromOpponent = false;

    static const std::vector<std::string> kInsults;
    static const std::vector<std::string> kAcceptances;

    void ShowRaceTypeDialog();
    void ShowDragBetDialog();
    void ShowRoadBetDialog();
    void ShowSpeech(const std::string& text);
    void Accept();
    void Reject();
    void OpponentRejectsBet();
};
