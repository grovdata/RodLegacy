#pragma once

#include "scenes/scene.h"
#include "race/driver.h"

#include <memory>

// The win ending: beat The King in the road race and the original plays this
// before the hall of fame. A cutscene on the garage screen - see
// RodLegacyResearch/docs/game_over.md ("The win ending") for the traced
// original (FUN_0023e062).
class Ending : public CarScene {
public:
    Ending();

    bool WantsPointer() const override { return false; }   // it is a cutscene
    void OnSceneEnter() override;
    void DrawScene() override;
    void OnFadeComplete() override;
    void OnCarAnimationComplete() override;
    SDL_Point GetCarIdlePosition() override;
    bool PushEvent(SDL_Event* event) override;

private:
    enum class Phase {
        Win,      // the full-screen picture with the ending text, any key moves on
        DriveIn,  // The King's car pulls into the garage
        Girl,     // 19 frames: she gets out
        Kiss,     // 9 frames: the kiss flies at the camera
        Done,     // held, any key leaves
        Leaving
    };

    Phase phase = Phase::Win;
    int frame = 1;
    int girlFrame = 1;   // held on her last frame once she is out

    Uint64 phaseStart = 0;

    Bitmap* girl = nullptr;    // 297 GIRLFRIEND_ANIMATION
    Bitmap* kiss = nullptr;    // 298 KISS_ANIMATION
    Bitmap* winPic = nullptr;  // 299 WIN_SCREEN, full screen with the text

    std::unique_ptr<CarBitmap> kingCarBitmap;
    // The King's car, but she is the one driving it in.
    Driver girlfriendDriver;
};
