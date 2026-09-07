#include "game_over_menu.h"
#include "state.h"
#include "save_load_scene.h"

namespace {

constexpr SDL_Rect kPanelRect  = {100, 50, 120, 90};
constexpr SDL_Rect kLoadRect   = {122, 67, 76, 18};
constexpr SDL_Rect kNewRect    = {122, 87, 76, 18};
constexpr SDL_Rect kQuitRect   = {122, 107, 76, 18};

constexpr SDL_Color kInk   = {0x1f, 0x1f, 0x20, 0xff};
constexpr SDL_Color kBlack = {0x00, 0x00, 0x00, 0xff};

std::unique_ptr<Button> MakeButton(const SDL_Rect& bounds, const char* text,
                                   std::function<void()> onClick) {
    ButtonConfig cfg{};
    cfg.bounds = bounds;
    cfg.text = text;
    cfg.onClick = std::move(onClick);
    cfg.borderColor = kInk;
    cfg.textColor = kInk;
    return std::make_unique<Button>(cfg);
}
}  // namespace

// The default Scene constructor: no background asset, so Init fills the screen
// black.
GameOverMenu::GameOverMenu() : Scene() {}

void GameOverMenu::Init(bool debugMode) {
    Scene::Init(debugMode);

    RectangleConfig panelCfg{};
    panelCfg.rect = kPanelRect;
    panelCfg.fillColor = kBlack;
    panelCfg.borderColor = kInk;
    panelCfg.inset = 1;
    panel = std::make_unique<Rectangle>(panelCfg);
    panel->SetVisible(true);

    loadButton = MakeButton(kLoadRect, "Load Game", [] {
        // Nowhere else to go back to if the player changes their mind.
        state.GetSaveLoadScene()->Configure(SaveLoadScene::Mode::Load, SCENE_GAME_OVER_MENU);
        state.MoveToScene(SCENE_SAVE_LOAD);
    });
    // Name entry is the start of a new game: it takes the name, then resets
    // bankroll and garage before dropping the player at the newspaper.
    newButton = MakeButton(kNewRect, "New Game", [] { state.MoveToScene(SCENE_NAME_ENTRY); });
    quitButton = MakeButton(kQuitRect, "Quit", [] {
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    });
}

void GameOverMenu::DrawScene() {
    Scene::DrawScene();
    panel->Draw();
    loadButton->Draw();
    newButton->Draw();
    quitButton->Draw();
}

bool GameOverMenu::PushEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        loadButton->HandleEvent(event);
        newButton->HandleEvent(event);
        quitButton->HandleEvent(event);
        return true;
    }
    return Scene::PushEvent(event);
}
