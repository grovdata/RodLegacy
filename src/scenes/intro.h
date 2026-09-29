#pragma once

#include "scenes/scene.h"
#include "scenes/intro_player.h"

#include <array>

class Intro : public Scene {
public:
    void Init(bool debugMode) override;
    bool WantsPointer() const override { return false; }
    void OnSceneEnter() override;
    void Update(Uint64 deltaTime) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnFadeComplete() override;

private:
    void PlaySound(IntroSound sound);
    void Leave();

    std::array<IntroImage, 6> images;
    IntroPlayer player;
    sdl2::TexturePtr frame;
    Uint64 tickAccumMs = 0;
    int plateTicks = 0;
    bool leaving = false;
    bool musicStarted = false;
};
