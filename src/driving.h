#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "racing.h"
#include "types.h"
#include "parsers/libp.h"
#include "ui/bubble.h"

#include <vector>
#include <memory>

enum DrivingState {
    DRIVING_STATE_NORMAL,
    DRIVING_STATE_SHIFTING,
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

private:
    void AddWheelDrawable(const std::string& id, Bitmap* bitmap);
    void LoadTrackData();
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
        bool PushEvent(SDL_Event *event) override;

        void SetState(DrivingState state);
        void OnFadeComplete() override;
        // Driven with the wheel/pedals; no pointer cursor in this scene.
        bool WantsPointer() const override { return false; }
};
