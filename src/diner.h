#pragma once

#include "scene.h"
#include "carbitmap.h"
#include "debug_overlay.h"
#include "animated_bitmap.h"
#include "dialogs/list_dialog.h"
#include "challenge.h"
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

    // Scene-owned animated drawables (asset sheets are owned by AssetManager).
    // Both actors are table-driven one-shots, not free-running loops - see
    // RodLegacyResearch/docs/diner.md.
    //
    // The phone guy's 20 frames live on two sheets, 0-9 and 10-19.
    std::unique_ptr<AnimatedBitmap> guyAnim[2];
    // The waitress has a sheet per direction: 0 walks right, 1 walks left.
    std::unique_ptr<AnimatedBitmap> waitressAnim[2];

    Opponent *currentOpponent;

    bool isPaused;

    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double lastFrameTime;
    double currentTime;

    // Waitress: -1 when she is not walking, otherwise the step into the
    // 84-entry frame table. dir picks the sheet and the run-length X table.
    int waitressStep = -1;
    int waitressDir = 0;
    bool waitressOnScreen = false;
    bool waitressTurnAround = false;  // walked in, rolled to walk straight out
    // The original gates her on a counter that has to reach 3, and raises the
    // odds the longer she has been away; this stands in for that.
    int waitressWaits = 0;
    float waitressStepTimer = 0.0f;

    // Phone guy: -1 when idle, otherwise the step into the 44-entry table.
    int guyStep = -1;
    float guyHoldTimer = 0.0f;

    float dinerRollTimer = 0.0f;

    // One pass of the original's animation loops, which wait on the display.
    // ponytail: eyeballed, one knob each - the guy's table holds each frame
    // for 2 to 8 of his steps, so his unit is much shorter than hers.
    static constexpr float WAITRESS_STEP_MS = 60.0f;
    static constexpr float GUY_STEP_MS = 120.0f;
    // How often the appear/leave/phone rolls are made. The original rolls once
    // per pass of the challenger dialog, which RodLegacy has no equivalent of.
    static constexpr float ROLL_SECONDS = 1.0f;
    bool engineSoundStarted = false;
    double debugWaitTimer = 0.0;

    std::unique_ptr<ListDialog> callListDialog;
    void ShowCallListDialog();

    std::unique_ptr<Challenge> challenge;

    public:
        Diner();
        ~Diner();
        void Init(bool debugMode) override;

        SDL_Point GetCarIdlePosition() override;

        void DrawScene() override;
        void DrawMaskAfterCar() override;
        // void SetStatusBar(char *text) override;

        void StartAnimateCar(CarPtr car, Driver *driver, CarAnimationType animType) override;
        void UpdateCarAnimation(double deltaTime) override;
        void UpdateAnimation(double deltaTime);
        void LoadActors();
        void UpdateActors(double deltaTime);
        void StartWaitressWalk(int dir);
        void ApplyWaitressFrame();
        void ApplyGuyFrame();


        void UpdateDebugInfo() override;
        void OnCarAnimationComplete() override;
        void OnCarExitComplete() override;

        bool PushEvent(SDL_Event *event) override;

        void OnFadeComplete() override;
        void OnSceneExit() override;
        void FadeOut() override;

        bool CanStartCarExit() const override { return !challenge->IsActive(); }
};
