#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "racing.h"
#include "types.h"

#include <vector>
#include <memory>

enum DrivingState {
    DRIVING_STATE_NORMAL,
    DRIVING_STATE_SHIFTING,
    DRIVING_STATE_TURNING_LEFT,
    DRIVING_STATE_TURNING_RIGHT    
};

class Driving: public Scene {
   private:

    std::unique_ptr<Racing> racing;

    SDL_Event *lastEvent;
    SDL_Rect roadRect = {0, 14, GAME_WIDTH, GAME_RACING_HEIGHT};
    SDL_Texture *roadTexture;

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

private:
    void AddWheelDrawable(const std::string& id, Bitmap* bitmap);

    public:
        Driving();
        ~Driving();

        void Init(bool debugMode) override;

        // MousePointer GetPointerType() override;

        // SDL_Point GetCarIdlePosition() override;
        // SDL_Point GetCarOffscreenPosition() override;

        // void Action(ZoneId zone) override;
        void Start();

        void Draw() override;
        void PushEvent(SDL_Event *event) override;

        void SetState(DrivingState state);
};
