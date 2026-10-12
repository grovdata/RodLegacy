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
    constexpr SDL_Rect kSetCheckRect = {114, 112, 14, 14};
    constexpr int kSetRowHeight = 16;
    constexpr int kSetLabelDx = kSetCheckRect.w + 4;
    constexpr int kSetLabelDy = 2;
    constexpr uint32_t kBlack = 0x00000000;
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

    // Offer a set only when its data is actually installed - switching on a
    // set with nothing behind it would add no cars and just look broken.
    setRows.clear();
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    const SrGame game = state.GetAssets().GetGame();
    for (size_t i = 0; i < defs.size(); ++i) {
        const int set = (int)i;
        if (CarSetIsOwnGame(defs[i], game)) continue;   // always on, no choice
        if (!state.GetAssets().HasCarSetData(set)) continue;

        auto row = std::make_unique<SetRow>();
        row->carSet = set;
        row->enabled = state.GetAssets().IsCarSetEnabled(set);

        SDL_Rect box = kSetCheckRect;
        box.y += (int)setRows.size() * kSetRowHeight;

        row->label.Init(font, box.x + kSetLabelDx, box.y + kSetLabelDy, kBlack);
        // A set that brings drivers brings its whole roster.
        row->label.SetText(defs[i].name + (defs[i].providesDrivers ? " cars & drivers"
                                                                   : " cars"));

        CheckboxConfig config;
        config.bounds = box;
        config.value = &row->enabled;
        row->checkbox = std::make_unique<Checkbox>(config);
        setRows.push_back(std::move(row));
    }
}

// The scene is re-entered when "Old Game" is cancelled, and leaving it stopped text input.
void NameEntry::OnSceneEnter() {
    Scene::OnSceneEnter();
    SDL_StartTextInput();
}

void NameEntry::RefreshNameLabel() {
    nameLabel.SetText(typedName + "_");
}

void NameEntry::HandleOk() {
    // Settle the car sets before StartNewGame, which rebuilds the spec and
    // driver tables this game's cars and opponents are generated from.
    if (!setRows.empty()) {
        for (const auto& row : setRows) {
            state.GetAssets().SetCarSetEnabled(row->carSet, row->enabled);
        }
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
    for (const auto& row : setRows) {
        row->label.Draw();
        row->checkbox->Draw();
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
        for (const auto& row : setRows) row->checkbox->HandleEvent(event);
        return true;
    }

    return Scene::PushEvent(event);
}

void NameEntry::OnSceneExit() {
    SDL_StopTextInput();
    Scene::OnSceneExit();
}
