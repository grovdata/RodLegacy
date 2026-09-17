#include "scenes/name_entry.h"
#include "core/state.h"
#include "scenes/save_load_scene.h"
#include "car/car_set.h"
#include "gfx/SFont.h"

namespace {
    constexpr int kBackgroundX = 19;
    constexpr int kBackgroundY = 6;
    constexpr int kNameTextX = 143;
    constexpr int kNameTextY = 65;
    constexpr SDL_Rect kOldGameRect = {150, 162, 66, 16};
    constexpr SDL_Rect kOkRect      = {222, 162, 66, 16};
    constexpr SDL_Rect kSeCheckRect = {32, 163, 14, 14};
    constexpr int kSeLabelX = 50;
    constexpr int kSeLabelY = 165;
    constexpr uint32_t kBlack = 0x00000000;

    // Index of the optional Street Rod SE set in GetCarSetDefs(), or -1.
    int SeCarSetIndex() {
        const std::vector<CarSetDef>& defs = GetCarSetDefs();
        for (size_t i = 0; i < defs.size(); ++i) {
            if (!defs[i].mandatory && defs[i].id == "srse") return (int)i;
        }
        return -1;
    }
}

NameEntry::NameEntry()
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, DMV_SCREEN}) {}

void NameEntry::Init(bool debugMode) {
    Scene::Init(debugMode);

    background->SetPosition(kBackgroundX, kBackgroundY);

    typedName.clear();

    SFont_Font* font = state.GetAssets().GetMainFont();
    
    nameLabel.Init(font, kNameTextX, kNameTextY, kBlack);
    RefreshNameLabel();

    ButtonConfig oldGameConfig{};
    oldGameConfig.bounds = kOldGameRect;
    oldGameConfig.text = "Old Game";
    oldGameConfig.onClick = []() {
        // Back here rather than to the garage when the player cancels: at this
        // point there is no game to return to yet.
        state.GetSaveLoadScene()->Configure(SaveLoadScene::Mode::Load, SCENE_NAME_ENTRY);
        state.MoveToScene(SCENE_SAVE_LOAD);
    };
    oldGameButton = std::make_unique<Button>(oldGameConfig);

    ButtonConfig okConfig{};
    okConfig.bounds = kOkRect;
    okConfig.text = "OK";
    okConfig.defaultChoice = true;
    okConfig.onClick = [this]() { HandleOk(); };
    okButton = std::make_unique<Button>(okConfig);

    // Offer the SE cars only when their data is actually installed - switching
    // on a set with nothing behind it would add no cars and just look broken.
    seCarsCheckbox.reset();
    const int seSet = SeCarSetIndex();
    if (seSet >= 0 && state.GetAssets().HasCarSetData(seSet)) {
        seCarsEnabled = state.GetAssets().IsCarSetEnabled(seSet);

        seCarsLabel.Init(font, kSeLabelX, kSeLabelY, kBlack);
        seCarsLabel.SetText("Street Rod SE cars");

        CheckboxConfig seConfig;
        seConfig.bounds = kSeCheckRect;
        seConfig.value = &seCarsEnabled;
        seCarsCheckbox = std::make_unique<Checkbox>(seConfig);
    }
}

// Init runs once, but the scene is re-entered when "Old Game" is cancelled,
// and leaving it stopped text input.
void NameEntry::OnSceneEnter() {
    Scene::OnSceneEnter();
    SDL_StartTextInput();
}

void NameEntry::RefreshNameLabel() {
    nameLabel.SetText(typedName + "_");
}

void NameEntry::HandleOk() {
    // Settle the car sets before StartNewGame, which rebuilds the spec table
    // this game's cars and opponents are generated from.
    const int seSet = SeCarSetIndex();
    if (seCarsCheckbox && seSet >= 0) {
        state.GetAssets().SetCarSetEnabled(seSet, seCarsEnabled);
        state.GetAssets().SaveSettings();
    }

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
    if (seCarsCheckbox) {
        seCarsLabel.Draw();
        seCarsCheckbox->Draw();
    }
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
        if (seCarsCheckbox) seCarsCheckbox->HandleEvent(event);
        return true;
    }

    return Scene::PushEvent(event);
}

void NameEntry::OnSceneExit() {
    SDL_StopTextInput();
    Scene::OnSceneExit();
}
