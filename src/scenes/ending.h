/*
 * RodLegacy - The win ending cutscene after beating The King.
 *
 * Created: 2026-09-15
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "ui/bubble.h"
#include "scenes/scene.h"
#include "race/driver.h"

#include <memory>

class Ending : public CarScene {
public:
    Ending();

    bool WantsPointer() const override { return phase == Phase::Win; }
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
        Done,     // held, then the girlfriend's line; any key leaves
        Leaving
    };

    Phase phase = Phase::Win;
    int frame = 1;
    int girlFrame = 1;

    Uint64 phaseStart = 0;
    std::unique_ptr<Bubble> heroBubble;   // shown a second after the kiss, any key leaves

    Bitmap* girl = nullptr;    // 297 GIRLFRIEND_ANIMATION
    Bitmap* kiss = nullptr;    // 298 KISS_ANIMATION
    SrLibColor winFill = {0, 0, 0, 255};
    Bitmap* winPic = nullptr;  // 299 WIN_SCREEN, full screen with the text

    std::unique_ptr<Bitmap> paintCan;   // the garage can, in the King's paint
    std::unique_ptr<CarBitmap> kingCarBitmap;
    // The King's car, but she is the one driving it in.
    Driver girlfriendDriver;
};
