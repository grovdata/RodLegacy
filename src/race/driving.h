/*
 * RodLegacy - The driving scene: Driving hosts races and town cruises, the cockpit, and settles each race.
 *
 * Created: 2023-02-28
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "race/racing.h"
#include "core/types.h"
#include "parsers/libp.h"
#include "ui/bubble.h"
#include "ui/label.h"
#include "dialogs/dialog.h"

#include <vector>
#include <memory>

enum DrivingState {
    DRIVING_STATE_NORMAL,
    DRIVING_STATE_TURNING_LEFT,
    DRIVING_STATE_TURNING_RIGHT,
    DRIVING_STATE_CRASHED
};

class Driving: public Scene {
   private:

    std::unique_ptr<Racing> racing;
public:
    void DebugShowTicket();
    // The live race renderer - State has a `racing` member of its own that is never constructed.
    Racing* GetRacing() const { return racing.get(); }
private:
    std::unique_ptr<TrackReader> trackReader;

    SDL_Event *lastEvent;
    SDL_Rect roadRect = {0, 14, GAME_WIDTH, GAME_RACING_HEIGHT};
    SDL_Texture *roadTexture;
    
    std::unique_ptr<Bitmap> crashOverlayBitmap;
    std::unique_ptr<Bitmap> crashBloodBitmap;
    bool lastCrashState = false;

    // Remembers whether the player had paused music, so leaving does not un-pause it.
    bool musicWasPaused = false;

    DrivingState currentState = DRIVING_STATE_NORMAL;

    bool raceEnded = false;
    bool raceEndPendingKing = false;  // King beaten in a drag -> end-game screen instead
    // The race ended the evening: the player drives home instead of staying at the diner.
    bool raceEndLeftTheDiner = false;
    std::unique_ptr<Bubble> raceEndBubble;
    // A crash holds the screen for a minimum time.
    Uint32 raceEndDismissAllowedMs = 0;
    // 31..90 mph wreck: a repair quote to pay or junk the car.
    std::unique_ptr<Dialog> wreckDialog;
    void ShowWreckDialog(const srrace::RaceEndContext& ctx);
    // The view fades out and back in before the message; its data is held for the dip.
    bool raceEndDipping = false;
    bool raceEndWon = false;
    bool raceEndWreck = false;
    srrace::RaceEndMessage raceEndReason;
    // Grudge Night's verdict on the round just run, carried the few lines from
    // where the night is told to where the race-end bubble is built.
    std::string grudgeResultLine;
    srrace::RaceEndContext raceEndCtx;
    void ShowRaceEndMessage();
    // Only when the race was decided at the line; snapshotted before the dip.
    std::unique_ptr<Dialog> statsDialog;
    bool raceEndShowStats = false;
    int raceEndTimeTenths = 0;
    int raceEndAvgSpeed = 0;
    int raceEndPercent = 0;
    void ShowStatsDialog();
    // The cop's ticket and his hand, with the player's name and car written on it.
    std::unique_ptr<Bitmap> ticketBitmap;
    std::unique_ptr<Bitmap> ticketHandBitmap;
    std::unique_ptr<Label> ticketName;
    std::unique_ptr<Label> ticketCar;
    bool ticketShown = false;
    void ShowTicket();
    void OnFadeComplete() override;
    void LeaveAfterRace();

public:
    // startSeg is the destination's start (10 hub / 65 diner / 115 gas station); runs to the arch at 380.
    void StartCruise(int startSeg, SrLibGameScene destination);
    static int CruiseStartSegment(SrLibGameScene destination);

private:
    bool cruising = false;
    SrLibGameScene cruiseDestination = SCENE_DINER;
    void EndCruise();
    void SettleRace(RaceOutcome outcome);
    void ApplyDrivetrainDamage(uint16_t endFlags);
    void ApplyRaceWear();
    // What is still owed for the new set; 0 unless the $50 could not be covered.
    int ApplyBlownTires();

    // SR2: wheel overlays (left, neutral, right) and the two grips.
    Bitmap* sr2Wheel[3] = {nullptr, nullptr, nullptr};
    Bitmap* sr2Grip[2] = {nullptr, nullptr};
    Bitmap* sr2Arm[2] = {nullptr, nullptr};
    void LoadSr2Cockpit();
    void SetSr2State(DrivingState state);
    Bitmap* wheelLeftHand1;
    Bitmap* wheelLeftHand2;
    Bitmap* wheelLeftHand3;
    Bitmap* wheelLeftHand4;
    Bitmap* wheelRightHand1;
    Bitmap* wheelRightHand2;
    Bitmap* wheelRightHand3;
    Bitmap* wheelRightHand4;
    Bitmap* wheelTurningLeft;
    Bitmap* wheelTurningRight;
    Bitmap* wheelTurningLeftOpposite;
    Bitmap* wheelTurningRightOpposite;

    int currentDiceIndex = 0;          // Current visible dice frame (0 = NEUTRAL..4)
    int diceTilt = 0;                  // Pendulum position -2..2
    int diceDir = 1;                   // Swing direction +-1
    float diceStepTimer = 0.0f;
    // The original steps once per race frame; this decouples the swing from our frame rate.
    static constexpr float DICE_SWING_STEP_MS = 130.0f;

    // Indexed 0 = automatic, 1 = 3-speed, 2 = 4-speed.
    Bitmap* shifterPlate[3] = {nullptr, nullptr, nullptr};
    Bitmap* shifterKnob[3] = {nullptr, nullptr, nullptr};
    Bitmap* shifterHand = nullptr;
    int shifterKind = 2;             // which of the three is on screen
    SDL_Point shifterOrigin{};       // plate top-left on screen
    // Nothing is drawn at rest: the dashboard has one painted in.
    enum class ShiftPhase { Idle, KnobStart, Slide, KnobEnd };
    ShiftPhase shiftPhase = ShiftPhase::Idle;
    int shiftStop = 0;               // where the knob rests, a gate-path index
    int shiftCursor = 0;             // where the hand is while a shift runs
    int shiftTarget = 0;
    int shiftStep = 0;
    float shiftStepTimer = 0.0f;
    // ponytail: the original steps once per race frame, whose rate is unknown; this is the calibration knob.
    static constexpr float SHIFT_STEP_MS = 130.0f;

private:
    void AddWheelDrawable(const std::string& id, Bitmap* bitmap);
    void ApplyCarLivery();
    void LoadTrackData();
    void LoadShifter();
    void ResetShifter();
    void UpdateShifter(float deltaTime);
    void UpdateDiceAnimation(float deltaTime);
    void DrawCrashOverlay();
    void DrawSteeringDot();

    public:
        Driving();
        ~Driving();

        void Init(bool debugMode) override;

        void Start();

        void DrawScene() override;
        void UpdateDebugInfo() override;
        std::vector<DebugPanel> GetDebugPanels() override;
        bool PushEvent(SDL_Event *event) override;

        void SetState(DrivingState state);
        void OnSceneEnter() override;
        void OnSceneExit() override;
        // No pointer, except for the wreck quote's buttons.
        bool WantsPointer() const override { return wreckDialog != nullptr; }
};
