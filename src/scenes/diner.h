#pragma once

#include "scenes/scene.h"
#include "car/carbitmap.h"
#include "debug/debug_overlay.h"
#include "gfx/animated_bitmap.h"
#include "dialogs/list_dialog.h"
#include "race/challenge.h"
class Opponent;
class EngineBayView;

class Diner: public CarScene {
   private:

    sdl2::SurfacePtr roadMaskSurface;
    sdl2::TexturePtr roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 155, 320, 10};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 10};

    sdl2::SurfacePtr maskedScreenBackground;
    sdl2::TexturePtr maskedScreenBackgroundTexture;

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    bool exitingCar;
    double waitTimer;

    std::string GetOpponentText(Opponent *opponent);

    // Keyed by car, not driver.
    std::map<CarPtr, std::unique_ptr<CarBitmap>> opponentCarBitmaps;

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
    // Stands in for the original's counter that must reach 3, with odds rising the longer she has been away.
    int waitressWaits = 0;
    float waitressStepTimer = 0.0f;
    // extra_animations only: >= 0 while she stands still partway in.
    float waitressPauseTimer = -1.0f;
    bool waitressPauseDone = true;

    // Jail: the diner behind bars, nothing clickable.
    Bitmap* jailBars = nullptr;

    // Phone guy: -1 when idle, otherwise the step into the 44-entry table.
    int guyStep = -1;
    float guyHoldTimer = 0.0f;

    float dinerRollTimer = 0.0f;

    // Each original step overruns its one-frame target on hardware; by eye a step is nearer three frames.
    static constexpr float WAITRESS_STEP_MS = 60.0f;
    static constexpr float GUY_STEP_MS = 120.0f;
    // The original rolls once per pass of a challenger dialog loop RodLegacy lacks.
    static constexpr float ROLL_SECONDS = 1.0f;
    bool engineSoundStarted = false;
    double debugWaitTimer = 0.0;
    // >= 0 while the leaving car is cranking; it holds still until the
    // starter sample has run. -1 = not cranking.
    double exitCrankTimer = -1.0;

    // False from the moment the starter cranks: the exit is committed and anything opened would outlive the car.
    bool OpponentPresent() const {
        return carBitmap && currentOpponent && !animatingCar && !exitingCar &&
               exitCrankTimer < 0.0;
    }

    std::unique_ptr<ListDialog> callListDialog;
    void ShowCallListDialog();

    std::unique_ptr<Challenge> challenge;

    // One timed look per challenger; asking again gets an earful.
    std::unique_ptr<EngineBayView> hoodView;
    float hoodTimer = 0.0f;
    bool hoodPopped = false;
    // ponytail: the original's timeout is unknown, replace once it can be read from the binary.
    static constexpr float HOOD_SECONDS = 5.0f;
    bool IsHoodViewOpen() const;
    void CloseHoodView();

    public:
        Diner();
        ~Diner();
        void Init(bool debugMode) override;

        SDL_Point GetCarIdlePosition() override;

        void DrawScene() override;
        void DrawMaskAfterCar() override;

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
        std::vector<DebugPanel> GetDebugPanels() override;

        void OnFadeComplete() override;
        void OnSceneExit() override;
        void FadeOut() override;

        // The challenger waits while his hood is up, same as during a challenge.
        bool CanStartCarExit() const override { return !challenge->IsActive() && !IsHoodViewOpen(); }

        // The one way the challenger leaves, so neither the idle timeout nor a finished challenge drives off silently.
        void StartOpponentExit();
};
