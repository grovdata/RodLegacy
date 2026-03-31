#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "racing.h"
#include "types.h"
#include "parsers/libp.h"

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
    bool lastCrashState = false; // Track previous crash state to avoid blinking

    DrivingState currentState = DRIVING_STATE_NORMAL;

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
    int currentDiceIndex = 0;          // Current visible dice (0-4)
    float diceAnimationTimer = 0.0f;   // Timer for dice animation
    bool diceAnimationForward = true;  // Direction: true = forward (1→5), false = backward (5→1)
    static constexpr float DICE_ANIMATION_INTERVAL = 1.5f; // Change dice every 1.5s when moving

private:
    void AddWheelDrawable(const std::string& id, Bitmap* bitmap);
    void LoadTrackData();
    void UpdateDiceAnimation(float deltaTime, bool carIsMoving);
    void DrawCrashOverlay();

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
        bool PushEvent(SDL_Event *event) override;

        void SetState(DrivingState state);
        void OnFadeComplete() override;
};
