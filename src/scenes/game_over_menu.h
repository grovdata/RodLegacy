/*
 * RodLegacy - The game-over menu: load, new game or quit.
 *
 * Created: 2026-09-07
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "ui/button.h"
#include "ui/rectangle.h"

#include <memory>

class GameOverMenu : public Scene {
public:
    GameOverMenu();

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;

private:
    std::unique_ptr<Rectangle> panel;
    std::unique_ptr<Button> loadButton;
    std::unique_ptr<Button> newButton;
    std::unique_ptr<Button> quitButton;
};
