#include "name_entry.h"
#include "state.h"
#include "SFont.h"

namespace {
constexpr int kNameTextX = 128;
constexpr int kNameTextY = 60;
constexpr SDL_Rect kOldGameRect = {70, 150, 80, 18};
constexpr SDL_Rect kOkRect      = {170, 150, 80, 18};
constexpr uint32_t kBlack = 0x00000000;
}

NameEntry::NameEntry()
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, DMV_SCREEN}) {}

void NameEntry::Init(bool debugMode) {
    Scene::Init(debugMode);

    typedName.clear();

    SFont_Font* font = state.GetAssets().GetMainFont();
    
    nameLabel.Init(font, kNameTextX, kNameTextY, kBlack);
    RefreshNameLabel();

    ButtonConfig oldGameConfig{};
    oldGameConfig.bounds = kOldGameRect;
    oldGameConfig.text = "Old Game";
    oldGameConfig.onClick = nullptr; // Unwired, matching Garage's own "Old Game" button.
    oldGameButton = std::make_unique<Button>(oldGameConfig);

    ButtonConfig okConfig{};
    okConfig.bounds = kOkRect;
    okConfig.text = "OK";
    okConfig.defaultChoice = true;
    okConfig.onClick = [this]() { HandleOk(); };
    okButton = std::make_unique<Button>(okConfig);

    SDL_StartTextInput();
}

void NameEntry::RefreshNameLabel() {
    nameLabel.SetText(typedName + "_");
}

void NameEntry::HandleOk() {
    state.SetPlayerName(typedName);
    state.SetBankroll(750);
    state.StartNewGame();
    state.MoveToScene(SCENE_NEWSPAPER);
}

void NameEntry::DrawScene() {
    Scene::DrawScene();
    nameLabel.Draw();    
    oldGameButton->Draw();
    okButton->Draw();
}

bool NameEntry::PushEvent(SDL_Event* event) {
    if (event->type == SDL_TEXTINPUT) {
        for (const char* p = event->text.text; *p; ++p) {
            if (typedName.size() >= MAX_NAME_LENGTH) break;
            if (*p >= 32 && *p < 127) {
                typedName.push_back(*p);
            }
        }
        RefreshNameLabel();
        return true;
    }

    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_BACKSPACE) {
            if (!typedName.empty()) {
                typedName.pop_back();
                RefreshNameLabel();
            }
            return true;
        }
        if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
            HandleOk();
            return true;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        oldGameButton->HandleEvent(event);
        okButton->HandleEvent(event);
        return true;
    }

    return Scene::PushEvent(event);
}

void NameEntry::OnSceneExit() {
    SDL_StopTextInput();
    Scene::OnSceneExit();
}
