/*
 * RodLegacy - Debug scene for auditioning every sound effect on each platform (debug_ui builds only).
 *
 * Created: 2026-10-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "audio/audio_manager.h"
#include "scenes/scene.h"
#include "ui/button.h"
#include "ui/label.h"

#include <memory>
#include <vector>

//   < >  or Left/Right   previous / next sound
//   A / P / D            play it as the Amiga, PC speaker or AdLib version
class SfxDebug : public Scene {
private:
    int current = 0;
    std::vector<std::unique_ptr<Button>> buttons;
    Label nameLabel;
    Label hintLabel;

    void Cycle(int delta);
    void Play(AudioManager::SfxSource src);
    void RefreshLabels();

public:
    SfxDebug() = default;

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
};
