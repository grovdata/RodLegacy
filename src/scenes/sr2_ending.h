/*
 * RodLegacy - Street Rod 2's ending: the car drives off into the sunset, then the closing text.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "scenes/sr2_end_script.h"

#include <array>
#include <vector>

class Sr2Ending : public Scene {
public:
    void Init(bool debugMode) override;
    bool WantsPointer() const override { return false; }
    void OnSceneEnter() override;
    void Update(Uint64 deltaTime) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnFadeComplete() override;

private:
    struct Image {
        int w = 0, h = 0;
        std::vector<uint8_t> px;
    };
    enum class Phase { Text, Animate, Hold, Leaving };

    void Compose();
    void Tick();
    void StartAnimation();
    void Leave();

    Image backdrop;     // libstart.p 33
    std::vector<uint8_t> stage;   // backdrop as the animation has altered it
    Image panel;        // 32, the closing text
    Image people;       // 34, the sprite sheet until the car drives off
    Image cars;         // 35, the sheet after that
    bool carSheet = false;
    Sr2EndScript script;
    std::vector<uint16_t> palette;
    bool ready = false;

    Phase phase = Phase::Text;
    size_t frame = 0;
    int ticksLeft = 0;
    Uint64 tickAccumMs = 0;
    std::array<uint8_t, 320 * 200> screen {};
    sdl2::TexturePtr texture320;
};
