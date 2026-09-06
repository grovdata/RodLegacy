#pragma once

#include "scene.h"
#include "bitmap.h"
#include "animated_bitmap.h"
#include "racing.h"
#include "types.h"
#include "parsers/libp.h"
#include "ui/bubble.h"

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
    std::unique_ptr<TrackReader> trackReader;

    SDL_Event *lastEvent;
    SDL_Rect roadRect = {0, 14, GAME_WIDTH, GAME_RACING_HEIGHT};
    SDL_Texture *roadTexture;
    
    // Crash overlay bitmap
    std::unique_ptr<Bitmap> crashOverlayBitmap;
    std::unique_ptr<Bitmap> crashBloodBitmap;
    bool lastCrashState = false; // Track previous crash state to avoid blinking

    // Music is silenced for the duration of a race; remember whether the player
    // had already paused it so leaving does not un-pause it for them.
    bool musicWasPaused = false;

    DrivingState currentState = DRIVING_STATE_NORMAL;

    // Race-end pause: freeze the scene, show the reason in a bubble, and wait for
    // any key to return to the garage.
    bool raceEnded = false;
    bool raceEndPendingKing = false;  // King beaten in a drag -> end-game screen instead
    std::unique_ptr<Bubble> raceEndBubble;
    // Earliest tick at which a keypress may dismiss the race-end screen. A crash
    // holds the screen for a minimum time so the crash view is actually seen.
    Uint32 raceEndDismissAllowedMs = 0;

    // Wheel drawable references (raw pointers - lifetime managed by drawable container)
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

    // Dice animation state
    int currentDiceIndex = 0;          // Current visible dice frame (0 = NEUTRAL..4)
    int diceTilt = 0;                  // Pendulum position -2..2 (original A4-0x1380)
    int diceDir = 1;                   // Swing direction +-1 (original A4-0x137e)
    float diceStepTimer = 0.0f;        // Accumulator for one +-1 pendulum step
    // Milliseconds per +-1 pendulum step (the original advances one step per race
    // frame; this decouples the swing rate from our display frame rate).
    static constexpr float DICE_SWING_STEP_MS = 130.0f;

    // Gear shifter - RodLegacyResearch/docs/driving/shifter.md.
    // Indexed 0 = automatic, 1 = 3-speed, 2 = 4-speed, the same order the
    // original's asset table at A4-0x42d4 uses.
    Bitmap* shifterPlate[3] = {nullptr, nullptr, nullptr};
    Bitmap* shifterKnob[3] = {nullptr, nullptr, nullptr};
    Bitmap* shifterHand = nullptr;
    int shifterKind = 2;             // which of the three is on screen
    SDL_Point shifterOrigin{};       // plate top-left on screen
    // Nothing of the gearbox is drawn at rest - the dashboard already has one
    // painted in. A shift shows the plate, the bare knob for one frame at each
    // end, and the hand on the knob for the slide between them.
    enum class ShiftPhase { Idle, KnobStart, Slide, KnobEnd };
    ShiftPhase shiftPhase = ShiftPhase::Idle;
    int shiftStop = 0;               // where the knob rests, a gate-path index
    int shiftCursor = 0;             // where the hand is while a shift runs
    int shiftTarget = 0;
    int shiftStep = 0;
    float shiftStepTimer = 0.0f;
    // ponytail: the original advances one animation phase per race frame, but
    // that frame rate is not pinned down; this is the calibration knob. Same
    // value as the dice swing, which is the other per-race-frame animation.
    static constexpr float SHIFT_STEP_MS = 130.0f;

private:
    void AddWheelDrawable(const std::string& id, Bitmap* bitmap);
    // Rebuilds the cockpit bitmaps that carry the car's own colour.
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

        // MousePointer GetPointerType() override;

        // SDL_Point GetCarIdlePosition() override;
        // SDL_Point GetCarOffscreenPosition() override;

        // void Action(ZoneId zone) override;
        void Start();

        void DrawScene() override;
        void UpdateDebugInfo() override;
        std::vector<DebugPanel> GetDebugPanels() override;
        bool PushEvent(SDL_Event *event) override;

        void SetState(DrivingState state);
        void OnSceneEnter() override;
        void OnSceneExit() override;
        // Driven with the wheel/pedals; no pointer cursor in this scene.
        bool WantsPointer() const override { return false; }
};
