#pragma once

#include "scene.h"
#include "carbitmap.h"
#include "debug_overlay.h"
#include "animated_bitmap.h"
class Opponent;

class Diner: public CarScene {
   private:

    sdl2::SurfacePtr roadMaskSurface;
    sdl2::TexturePtr roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 155, 320, 10}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 10};

    sdl2::SurfacePtr maskedScreenBackground;
    sdl2::TexturePtr maskedScreenBackgroundTexture;

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    bool exitingCar;
    double waitTimer;

    std::string GetOpponentText(Opponent *opponent);

    // Add a map to store opponent car bitmaps
    std::map<std::string, std::unique_ptr<CarBitmap>> opponentCarBitmaps;

    // Scene-owned animated drawables (asset sheets are owned by AssetManager)
    std::unique_ptr<AnimatedBitmap> phonebookGuyAnim;
    std::unique_ptr<AnimatedBitmap> waitressLeftAnim;
    std::unique_ptr<AnimatedBitmap> waitressRightAnim;

    Opponent *currentOpponent;

    bool isPaused;

    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double lastFrameTime;
    double currentTime;

    // Waitress animation state
    enum WaitressState {
        WAITRESS_ENTERING,
        WAITRESS_PAUSED,
        WAITRESS_EXITING,
        WAITRESS_HIDDEN
    };
    WaitressState waitressState = WAITRESS_ENTERING;
    double waitressAnimTime = 0.0;
    double waitressPauseTime = 2.0; // 2 second pause
    double waitressSpeed = 100.0; // pixels per second
    bool engineSoundStarted = false;
    double debugWaitTimer = 0.0;

    public:
        Diner();
        ~Diner();
        void Init(bool debugMode) override;

        SDL_Point GetCarIdlePosition() override;

        void Action(ZoneId zone) override;

        void Draw() override;
        void DrawMaskAfterCar() override;
        // void SetStatusBar(char *text) override;

        void StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType) override;
        void UpdateCarAnimation(double deltaTime) override;
        void UpdateAnimation(double deltaTime);
        void UpdateWaitressSequence(double deltaTime, AnimatedBitmap* waitressLeft, AnimatedBitmap* waitressRight);


        void UpdateDebugInfo() override;
        void OnCarAnimationComplete() override;
        void OnCarExitComplete() override;

        void PushEvent(SDL_Event *event) override;

        void OnFadeComplete() override;
        void OnSceneExit() override;
        void FadeOut() override;
};
