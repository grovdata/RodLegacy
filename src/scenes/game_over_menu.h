#pragma once

#include "scenes/scene.h"
#include "ui/button.h"
#include "ui/rectangle.h"

#include <memory>

// Where every game-over sequence ends up, straight after the hall of fame
// (KingOfCounty) - whether the run ended in the slammer or by beating The
// King. A bare black screen with one panel: Load Game, New Game, Quit. There
// is deliberately no way back into the run; it is over.

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
