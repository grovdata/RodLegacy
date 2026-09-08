#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"
#include "embedded_loader.h"
#include "ui/main_font.h"
#include "audio/audio_manager.h"
#include "engine_part.h"
#include "parsers/libp.h"
#include "save_load_scene.h"
#include <SDL_image.h>
#include <algorithm>

void Garage::InitGameMenuDialog() {

    const int buttonWidth = 66;
    const int buttonHeight = 16;


    SDL_Rect menuDialogRect = {82, 77, 156, 76};

    SDL_Rect saveGameRect = {92, 94, buttonWidth, buttonHeight};
    SDL_Rect oldGameRect = {92, 112, buttonWidth, buttonHeight};
    SDL_Rect newGameRect = {92, 130, buttonWidth, buttonHeight};

    SDL_Rect optionsRect = {164, 94, buttonWidth, buttonHeight};
    SDL_Rect quitGameRect = {164, 112, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {164, 130, buttonWidth, buttonHeight};

    Zone saveGameZone("save_game", "Save Game", saveGameRect);
    Zone oldGameZone("old_game", "Old Game", oldGameRect);
    Zone newGameZone("new_game", "New Game", newGameRect);
    Zone optionsZone("options", "Options", optionsRect);
    Zone quitGameZone("quit", "Quit", quitGameRect);
    Zone forgetItZone("forget_it", "Forget it", forgetItRect);

    std::vector<Zone> dialogZones = {
        saveGameZone,
        oldGameZone,
        newGameZone,
        optionsZone,
        quitGameZone,
        forgetItZone,
    };

    ButtonConfig saveGameButton = {
        saveGameRect,
        "Save Game",
        nullptr,
        true,
        0
    };

    ButtonConfig oldGameButton = {
        oldGameRect,
        "Old Game",
        nullptr,
        false,
        1
    };

    ButtonConfig newGameButton = {
        newGameRect,
        "New Game",
        nullptr,
        false,
        2
    };

    ButtonConfig optionsButton = {
        optionsRect,
        "Options",
        nullptr,
        false,
        5
    };

    ButtonConfig quitGameButton = {
        quitGameRect,
        "Quit",
        nullptr,
        false,
        3
    };

    ButtonConfig forgetItButton = {
        forgetItRect,
        "Forget it",
        nullptr,
        false,
        4
    };


    std::vector<ButtonConfig> buttons = {
        saveGameButton,
        oldGameButton,
        newGameButton,
        quitGameButton,
        forgetItButton,
        optionsButton
    };

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{menuDialogRect.x + 3, menuDialogRect.y + 3, 150, 10}, "Save, Restart, or Quit.", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {85, 89, 150, 1},
    };


    DialogConfig menuDialogConfig;
    menuDialogConfig.bounds = menuDialogRect;
    menuDialogConfig.backgroundColor = {0, 208, 0, 255};
    menuDialogConfig.borderColor = {0, 111, 239, 255};
    menuDialogConfig.texts = texts;
    menuDialogConfig.buttons = buttons;
    menuDialogConfig.lines = lines;
    menuDialogConfig.zones = dialogZones;
    menuDialogConfig.contentAreas = std::vector<SDL_Rect>{menuDialogRect};
    menuDialogConfig.font = state.GetAssets().GetMainFont();
    menuDialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    menuDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Save Game");
                gameMenuDialog->Hide();
                SetActiveDialog(nullptr);
                state.GetSaveLoadScene()->Configure(SaveLoadScene::Mode::Save, SCENE_GARAGE);
                state.MoveToScene(SCENE_SAVE_LOAD);
                break;
            case 1:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Load Game");
                gameMenuDialog->Hide();
                SetActiveDialog(nullptr);
                state.GetSaveLoadScene()->Configure(SaveLoadScene::Mode::Load, SCENE_GARAGE);
                state.MoveToScene(SCENE_SAVE_LOAD);
                break;
            case 2:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: New Game");
                gameMenuDialog->Hide();
                state.MoveToScene(SCENE_NAME_ENTRY);
                break;
            case 3: {
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Quit Game");
                SDL_Event event = {};
                event.type = SDL_QUIT;
                SDL_PushEvent(&event);
                break;
            }
            case 4:
                gameMenuDialog->Hide();
                break;
            case 5:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Options");
                gameMenuDialog->Hide();
                InitOptionsDialog();
                SetActiveDialog(optionsDialog.get());
                break;
        }
    };
    gameMenuDialog = std::make_unique<Dialog>(menuDialogConfig);
}

void Garage::InitOptionsDialog() {
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int labelWidth = 80;
    const int checkboxSize = 14;

    AudioManager* am = state.GetAssets().GetAudioManager();

    // Sound/music rows only make sense when an AudioManager exists -- it's
    // null for the whole session under --mute, and the volume buttons would
    // just silently no-op (nothing to set the volume on).
    bool hasAudio = am != nullptr;

    // Show music source row whenever Amiga music is loaded (always).
    // The button only cycles if DOS music is also available.
    bool showMusicSource = am && am->HasAmigaMusic();
    bool canCycleMusicSource = showMusicSource && am->HasPCSpeaker();

    // Rows are laid out top-down as a cursor so any row can be skipped
    // (no audio manager, no music source) without leaving a blank gap.
    int y = 70;
    const int rowHeight = 20;

    SDL_Rect optionsDialogRect = {60, 50, 200, 0};  // height fixed up below

    std::vector<Zone> dialogZones;
    std::vector<DialogTextConfig> texts = {
        {SDL_Rect{optionsDialogRect.x, optionsDialogRect.y + 3, optionsDialogRect.w, 10}, "Options", {0xFF,0xFF,0xFF,0xFF}, true},
    };
    std::vector<ButtonConfig> buttons;
    std::vector<Button*> buttonPtrs;

    char sfxLabel[32], musicLabel[32];
    if (hasAudio) {
        int sfxVol = am->GetSfxVolume();
        snprintf(sfxLabel, sizeof(sfxLabel), "Sound: %d%%", sfxVol * 10);
        SDL_Rect soundVolumeUpRect   = {190, y, 20, buttonHeight - 3};
        SDL_Rect soundVolumeDownRect = {165, y, 20, buttonHeight - 3};
        texts.push_back({SDL_Rect{optionsDialogRect.x + 5, y, labelWidth, 10}, sfxLabel, {0xFF,0xFF,0xFF,0xFF}, false});
        dialogZones.push_back(Zone("sound_up",   "[", soundVolumeUpRect));
        dialogZones.push_back(Zone("sound_down", "]", soundVolumeDownRect));
        buttons.push_back({soundVolumeUpRect,   "[", nullptr, false, 0});
        buttons.push_back({soundVolumeDownRect, "]", nullptr, false, 1});
        y += rowHeight;

        int musicVol = am->GetMusicVolume();
        snprintf(musicLabel, sizeof(musicLabel), "Music: %d%%", musicVol * 10);
        SDL_Rect musicVolumeUpRect   = {190, y, 20, buttonHeight - 3};
        SDL_Rect musicVolumeDownRect = {165, y, 20, buttonHeight - 3};
        texts.push_back({SDL_Rect{optionsDialogRect.x + 5, y, labelWidth, 10}, musicLabel, {0xFF,0xFF,0xFF,0xFF}, false});
        dialogZones.push_back(Zone("music_up",   "]", musicVolumeUpRect));
        dialogZones.push_back(Zone("music_down", "[", musicVolumeDownRect));
        buttons.push_back({musicVolumeUpRect,   "[", nullptr, false, 2});
        buttons.push_back({musicVolumeDownRect, "]", nullptr, false, 3});
        y += rowHeight;
    }

    SDL_Rect fullscreenCheckboxRect = {optionsDialogRect.x + labelWidth + 10, y - 1, checkboxSize, checkboxSize};
    texts.push_back({SDL_Rect{optionsDialogRect.x + 5, y, labelWidth, 10}, "Fullscreen:", {0xFF,0xFF,0xFF,0xFF}, false});
    dialogZones.push_back(Zone("fullscreen", "Toggle Fullscreen", fullscreenCheckboxRect));
    y += rowHeight;

    CheckboxConfig fullscreenCheckboxConfig;
    fullscreenCheckboxConfig.bounds = fullscreenCheckboxRect;
    fullscreenCheckboxConfig.value = &isFullScreen;
    fullscreenCheckboxConfig.fillColor = {0, 208, 0, 255};
    fullscreenCheckboxConfig.borderColor = {0, 0, 0, 255};
    fullscreenCheckboxConfig.checkColor = {0, 0, 0, 255};
    fullscreenCheckboxConfig.index = 4;
    fullscreenCheckboxConfig.onChange = [](bool checked) {
        LOG_INFO("Options: Fullscreen %s", checked ? "enabled" : "disabled");
        SDL_SetWindowFullscreen(win, checked ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    };
    buttonPtrs.push_back(new Checkbox(fullscreenCheckboxConfig));

    char srcLabel[32];
    if (showMusicSource) {
        const char* srcName = (am->GetMusicSource() == AudioManager::MusicSource::DOS) ? "DOS" : "Amiga";
        snprintf(srcLabel, sizeof(srcLabel), "%s", srcName);
        SDL_Rect musicSourceRect = {optionsDialogRect.x + labelWidth + 10, y, 50, buttonHeight};
        texts.push_back({SDL_Rect{optionsDialogRect.x + 5, y, labelWidth, 10}, "Soundtrack:", {0xFF,0xFF,0xFF,0xFF}, false});
        dialogZones.push_back(Zone("music_source", "Switch music version", musicSourceRect));
        buttons.push_back({musicSourceRect, srcLabel, nullptr, false, 6});
        y += rowHeight;
    }

    SDL_Rect backRect = {165, y, buttonWidth, buttonHeight};
    dialogZones.push_back(Zone("back", "Forget it", backRect));
    buttons.push_back({backRect, "Forget it", nullptr, true, 5});
    y += rowHeight;

    optionsDialogRect.h = y - optionsDialogRect.y;

    std::vector<SDL_Rect> lines = {
        {63, 62, 194, 1},
    };

    DialogConfig optionsDialogConfig;
    optionsDialogConfig.bounds          = optionsDialogRect;
    optionsDialogConfig.backgroundColor = {0, 208, 0, 255};
    optionsDialogConfig.borderColor     = {0, 111, 239, 255};
    optionsDialogConfig.texts           = texts;
    optionsDialogConfig.buttons         = buttons;
    optionsDialogConfig.buttonPtrs      = buttonPtrs;
    optionsDialogConfig.lines           = lines;
    optionsDialogConfig.zones           = dialogZones;
    optionsDialogConfig.contentAreas    = std::vector<SDL_Rect>{optionsDialogRect};
    optionsDialogConfig.font            = state.GetAssets().GetMainFont();
    optionsDialogConfig.onZoneHover     = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    optionsDialogConfig.onButtonPressed = [this, canCycleMusicSource](int buttonIndex) {
        AudioManager* am = state.GetAssets().GetAudioManager();
        LOG_INFO("Options: button %d pressed, AudioManager %s", buttonIndex, am ? "present" : "NULL");
        switch (buttonIndex) {
            case 0:
                if (am) { am->SetSfxVolume(am->GetSfxVolume() + 1); state.GetAssets().SaveSettings(); rebuildOptions_ = true; }
                break;
            case 1:
                if (am) { am->SetSfxVolume(am->GetSfxVolume() - 1); state.GetAssets().SaveSettings(); rebuildOptions_ = true; }
                break;
            case 2:
                if (am) { am->SetMusicVolume(am->GetMusicVolume() + 1); state.GetAssets().SaveSettings(); rebuildOptions_ = true; }
                break;
            case 3:
                if (am) { am->SetMusicVolume(am->GetMusicVolume() - 1); state.GetAssets().SaveSettings(); rebuildOptions_ = true; }
                break;
            case 5:
                optionsDialog->Hide();
                SetActiveDialog(gameMenuDialog.get());
                break;
            case 6:
                if (am && canCycleMusicSource) {
                    AudioManager::MusicSource next =
                        (am->GetMusicSource() == AudioManager::MusicSource::AMIGA)
                        ? AudioManager::MusicSource::DOS
                        : AudioManager::MusicSource::AMIGA;
                    am->SetMusicSource(next);
                    state.GetAssets().SaveSettings();
                    rebuildOptions_ = true;
                }
                break;
        }
    };

    optionsDialog = std::make_unique<Dialog>(optionsDialogConfig);
}

void Garage::InitBumperDialogs() {
}

void Garage::ShowRadioDialog() {
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (!am || !am->HasActiveMusic()) return;

    const bool multiSong = am->GetSongCount() > 1;

    const int dw = 144, dh = 46;
    const int dx = (320 - dw) / 2, dy = 60;
    SDL_Rect dlgRect = {dx, dy, dw, dh};

    // Button row at bottom of dialog
    const int btnY  = dy + dh - 18;
    const int btnH  = 14;
    SDL_Rect prevRect  = {dx + 4,          btnY, 24, btnH};
    SDL_Rect ppRect    = {dx + 32,         btnY, 50, btnH};
    SDL_Rect nextRect  = {dx + 86,         btnY, 24, btnH};
    SDL_Rect closeRect = {dx + dw - 28,    btnY, 24, btnH};

    Zone prevZone ("radio_prev",  "Previous song", prevRect);
    Zone ppZone   ("radio_pp",   "Play / Pause",  ppRect);
    Zone nextZone ("radio_next", "Next song",     nextRect);
    Zone closeZone("radio_close","Close",          closeRect);

    std::string songName = am->GetCurrentSongName();
    char nameLabel[64];
    if (multiSong) {
        snprintf(nameLabel, sizeof(nameLabel), "%s (%d/%d)",
                 songName.c_str(),
                 am->GetCurrentSongIndex() + 1,
                 am->GetSongCount());
    } else {
        snprintf(nameLabel, sizeof(nameLabel), "%s", songName.c_str());
    }

    const bool paused = am->IsMusicPaused();

    std::vector<DialogTextConfig> texts = {
        {SDL_Rect{dx, dy + 4, dw, 10}, nameLabel, {0xFF, 0xFF, 0xFF, 0xFF}, true},
    };

    std::vector<SDL_Rect> lines = {
        {dx + 2, btnY - 3, dw - 4, 1},
    };

    ButtonConfig prevBtn  = {prevRect,  "<<", nullptr, false, 0};
    ButtonConfig ppBtn    = {ppRect, paused ? "Play" : "Pause", nullptr, false, 1};
    ButtonConfig nextBtn  = {nextRect,  ">>", nullptr, false, 2};
    ButtonConfig closeBtn = {closeRect, "X",  nullptr, false, 3};

    // Disable prev/next for single-track sources
    if (!multiSong) {
        prevBtn.text = "";
        nextBtn.text = "";
    }

    DialogConfig cfg;
    cfg.bounds          = dlgRect;
    cfg.backgroundColor = {0, 208, 0, 255};
    cfg.borderColor     = {0, 111, 239, 255};
    cfg.font            = state.GetAssets().GetMainFont();
    cfg.texts           = texts;
    cfg.lines           = lines;
    cfg.buttons         = {prevBtn, ppBtn, nextBtn, closeBtn};
    cfg.zones           = {prevZone, ppZone, nextZone, closeZone};
    cfg.contentAreas    = {dlgRect};
    cfg.onZoneHover     = [this](Zone& zone) { hud->SetText(zone.GetText().c_str()); };
    cfg.onDismiss       = [this]() {
        radioDialog->Hide();
        SetActiveDialog(nullptr);
    };
    cfg.onButtonPressed = [this](int idx) {
        AudioManager* am = state.GetAssets().GetAudioManager();
        if (!am) return;
        switch (idx) {
            case 0: am->PrevSong();   rebuildRadio_ = true; break;
            case 1:
                if (am->IsMusicPaused()) am->ResumeMusic();
                else                     am->PauseMusic();
                rebuildRadio_ = true;
                break;
            case 2: am->NextSong();   rebuildRadio_ = true; break;
            case 3:
                radioDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };

    if (radioDialog && radioDialog->IsVisible())
        radioDialog->Hide();
    radioDialog = std::make_unique<Dialog>(cfg);
    radioDialog->Show();
    SetActiveDialog(radioDialog.get());
}
