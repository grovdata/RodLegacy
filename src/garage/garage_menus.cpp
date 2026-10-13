#include "garage/garage.h"
#include "race/race_sim.h"
#include "dialogs/key_bindings_dialog.h"
#include "dialogs/settings_dialog.h"
#include "core/state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "core/assets.h"
#include "core/state.h"
#include "car/engine_bay.h"
#include "core/embedded_loader.h"
#include "ui/main_font.h"
#include "audio/audio_manager.h"
#include "core/video.h"
#include "car/engine_part.h"
#include "parsers/libp.h"
#include "scenes/save_load_scene.h"
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
    forgetItButton.cancelChoice = true;


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

static const char* MusicSourceLabel(AudioManager::MusicSource src) {
    switch (src) {
        case AudioManager::MusicSource::DOS:   return "PC spkr";
        case AudioManager::MusicSource::ADLIB: return "AdLib";
        case AudioManager::MusicSource::C64:   return "C64";
        case AudioManager::MusicSource::AMIGA: return "Amiga";
        default:                               return "None";
    }
}

void Garage::InitOptionsDialog() {
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int arrowWidth = 20;
    const int arrowHeight = 14;

    AudioManager* am = state.GetAssets().GetAudioManager();

    // AudioManager is null under --mute.
    bool hasAudio = am != nullptr;

    // Rows advance as a cursor so skipped rows leave no gap; starting at y 24 keeps them clear of the HUD.
    SDL_Rect optionsDialogRect = {60, 24, 200, 0};

    const int pad = 5;
    const int innerLeft  = optionsDialogRect.x + pad;
    const int innerRight = optionsDialogRect.x + optionsDialogRect.w - pad;
    const int labelWidth = innerRight - innerLeft;
    const int rowHeight = 18;
    const int fontHeight = SFont_TextHeight(state.GetAssets().GetMainFont());

    int y = optionsDialogRect.y + 17;

    auto rowY = [&](int h) { return y + (rowHeight - h) / 2; };

    std::vector<Zone> dialogZones;
    std::vector<DialogTextConfig> texts = {
        {SDL_Rect{optionsDialogRect.x, optionsDialogRect.y + 3, optionsDialogRect.w, 10}, "Options", {0xFF,0xFF,0xFF,0xFF}, true},
    };
    std::vector<ButtonConfig> buttons;
    std::vector<Button*> buttonPtrs;

    char speedLabel[32];
    const int raceSpeed = state.GetAssets().GetRaceSpeed();
    if (raceSpeed == srrace::SIM_HZ_ORIGINAL) {
        snprintf(speedLabel, sizeof(speedLabel), "Race speed: %d (orig)", raceSpeed);
    } else {
        snprintf(speedLabel, sizeof(speedLabel), "Race speed: %d", raceSpeed);
    }
    SDL_Rect speedUpRect   = {innerRight - arrowWidth, rowY(arrowHeight), arrowWidth, arrowHeight};
    SDL_Rect speedDownRect = {speedUpRect.x - arrowWidth - 4, speedUpRect.y, arrowWidth, arrowHeight};
    texts.push_back({SDL_Rect{innerLeft, rowY(fontHeight), labelWidth, 10}, speedLabel, {0xFF,0xFF,0xFF,0xFF}, false});
    dialogZones.push_back(Zone("race_speed_up",   "Faster races", speedUpRect));
    dialogZones.push_back(Zone("race_speed_down", "Slower races, 6 is the original", speedDownRect));
    buttons.push_back({speedUpRect,   "[", nullptr, false, 9});
    buttons.push_back({speedDownRect, "]", nullptr, false, 10});
    y += rowHeight;

    SDL_Rect videoRect = {innerLeft, rowY(buttonHeight), labelWidth, buttonHeight};
    dialogZones.push_back(Zone("video", "Window size and fullscreen", videoRect));
    buttons.push_back({videoRect, "Video", nullptr, false, 12});
    y += rowHeight;

    if (hasAudio) {
        SDL_Rect soundRect = {innerLeft, rowY(buttonHeight), labelWidth, buttonHeight};
        dialogZones.push_back(Zone("sound", "Volume, soundtrack and engine sound", soundRect));
        buttons.push_back({soundRect, "Sound", nullptr, false, 11});
        y += rowHeight;
    }

    // Full dialog width: "Key bindings" does not fit the narrow button column.
    SDL_Rect keyBindingsRect = {innerLeft, rowY(buttonHeight), labelWidth, buttonHeight};
    dialogZones.push_back(Zone("key_bindings", "See every control", keyBindingsRect));
    buttons.push_back({keyBindingsRect, "Key bindings", nullptr, false, 7});
    y += rowHeight;

    SDL_Rect settingsRect = {innerLeft, rowY(buttonHeight), labelWidth, buttonHeight};
    dialogZones.push_back(Zone("settings", "Settings with no other UI", settingsRect));
    buttons.push_back({settingsRect, "Settings", nullptr, false, 8});
    y += rowHeight;

    SDL_Rect backRect = {innerRight - buttonWidth, rowY(buttonHeight), buttonWidth, buttonHeight};
    dialogZones.push_back(Zone("back", "Forget it", backRect));
    ButtonConfig backButton = {backRect, "Forget it", nullptr, true, 5};
    backButton.cancelChoice = true;  // only real action here - Enter and Escape both go back
    buttons.push_back(backButton);
    y += rowHeight;

    optionsDialogRect.h = y - optionsDialogRect.y + pad - 1;

    std::vector<SDL_Rect> lines = {
        {optionsDialogRect.x + 3, optionsDialogRect.y + 12, optionsDialogRect.w - 6, 1},
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
    optionsDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 5:
                optionsDialog->Hide();
                SetActiveDialog(gameMenuDialog.get());
                break;
            case 9:
            case 10:
                state.GetAssets().SetRaceSpeed(state.GetAssets().GetRaceSpeed() + (buttonIndex == 9 ? 1 : -1));
                state.GetAssets().SaveSettings();
                rebuildOptions_ = true;
                break;
            case 7:
                optionsDialog->Hide();
                ShowKeyBindingsDialog();
                break;
            case 8:
                optionsDialog->Hide();
                ShowSettingsDialog();
                break;
            case 11:
                optionsDialog->Hide();
                InitSoundDialog();
                SetActiveDialog(soundDialog.get());
                break;
            case 12:
                optionsDialog->Hide();
                InitVideoDialog();
                SetActiveDialog(videoDialog.get());
                break;
        }
    };

    optionsDialog = std::make_unique<Dialog>(optionsDialogConfig);
}

// Only whole-number scales of 320x200 that fit the display are offered, one row each.
void Garage::InitVideoDialog() {
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int checkboxSize = 14;
    const int listRow = 10;
    const int maxRows = 6;
    const int scrollW = 15;
    const SDL_Color black = {0x00, 0x00, 0x00, 0xFF};

    const int maxScale = video::MaxScale();
    std::vector<std::string> labels;
    for (int s = 1; s <= maxScale; ++s) {
        labels.push_back(std::to_string(s) + "x   " + std::to_string(GAME_WIDTH * s) + "x" +
                         std::to_string(GAME_HEIGHT * s));
    }

    // What is on screen now: the window, or in fullscreen the desktop and the whole-scale picture inside it.
    std::string now;
    SDL_Rect desktop = {0, 0, 0, 0};
    const int display = std::max(0, SDL_GetWindowDisplayIndex(win));
    if (video::Fullscreen() && SDL_GetDisplayBounds(display, &desktop) == 0) {
        const int s = video::MaxScaleFor(desktop.w, desktop.h, 0);
        now = "Now: " + std::to_string(desktop.w) + "x" + std::to_string(desktop.h) + ", picture " +
              std::to_string(GAME_WIDTH * s) + "x" + std::to_string(GAME_HEIGHT * s);
    } else {
        now = "Now: " + std::to_string(GAME_WIDTH * video::Scale()) + "x" +
              std::to_string(GAME_HEIGHT * video::Scale());
    }

    SDL_Rect rect = {60, 24, 200, 0};
    const int pad = 5;
    const int innerLeft  = rect.x + pad;
    const int innerRight = rect.x + rect.w - pad;
    const int labelWidth = innerRight - innerLeft;
    const int rowHeight = 18;
    const int fontHeight = SFont_TextHeight(state.GetAssets().GetMainFont());

    int y = rect.y + 17;
    auto rowY = [&](int h) { return y + (rowHeight - h) / 2; };

    std::vector<Zone> zones;
    std::vector<DialogTextConfig> texts = {{SDL_Rect{rect.x, rect.y + 3, rect.w, 10}, "Video", black, true}};
    std::vector<ButtonConfig> buttons;
    std::vector<Button*> buttonPtrs;

    texts.push_back({SDL_Rect{innerLeft, y + 2, labelWidth, 10}, "Window size:", black, false});
    y += 13;
    // Tall enough for the four scroll buttons; a 4K screen offers ten sizes.
    SDL_Rect listBox = {innerLeft + 4, y, labelWidth - scrollW - 10, std::max(maxRows * listRow, 4 * (scrollW + 1))};
    const int scrollX = innerRight - scrollW;
    SDL_Rect scrollTop    = {scrollX, listBox.y, scrollW, scrollW};
    SDL_Rect scrollUp     = {scrollX, listBox.y + scrollW + 1, scrollW, scrollW};
    SDL_Rect scrollDown   = {scrollX, listBox.y + listBox.h - 2 * scrollW - 1, scrollW, scrollW};
    SDL_Rect scrollBottom = {scrollX, listBox.y + listBox.h - scrollW, scrollW, scrollW};
    zones.push_back(Zone("scale_list", video::Fullscreen()
                                           ? "Window size, used when fullscreen is off"
                                           : "Window size, whole multiples of 320x200 that fit the screen",
                         listBox));
    y += listBox.h + 4;

    texts.push_back({SDL_Rect{innerLeft, y, labelWidth, 10}, now, black, false});
    y += 12;

    SDL_Rect fullRect = {innerRight - checkboxSize, rowY(checkboxSize), checkboxSize, checkboxSize};
    texts.push_back({SDL_Rect{innerLeft, rowY(fontHeight), labelWidth, 10}, "Fullscreen:", black, false});
    zones.push_back(Zone("fullscreen", "Fill the screen at the largest whole scale (F11)", fullRect));
    CheckboxConfig fullCfg;
    fullCfg.bounds = fullRect;
    fullCfg.value = &isFullScreen;
    fullCfg.index = 2;
    fullCfg.onChange = [this](bool checked) {
        video::SetFullscreen(checked);
        video::Save();
        rebuildVideo_ = true;
    };
    buttonPtrs.push_back(new Checkbox(fullCfg));
    y += rowHeight;

    SDL_Rect backRect = {innerRight - buttonWidth, rowY(buttonHeight), buttonWidth, buttonHeight};
    zones.push_back(Zone("back", "Forget it", backRect));
    ButtonConfig backButton = {backRect, "Forget it", nullptr, true, 3};
    backButton.cancelChoice = true;
    buttons.push_back(backButton);
    y += rowHeight;

    rect.h = y - rect.y + pad - 1;

    DialogConfig cfg;
    cfg.bounds          = rect;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor     = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.texts           = texts;
    cfg.buttons         = buttons;
    cfg.buttonPtrs      = buttonPtrs;
    cfg.lines           = {{rect.x + 3, rect.y + 12, rect.w - 6, 1}};
    cfg.zones           = zones;
    cfg.contentAreas    = std::vector<SDL_Rect>{rect};
    cfg.font            = state.GetAssets().GetMainFont();
    cfg.onZoneHover     = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onButtonPressed = [this](int buttonIndex) {
        if (buttonIndex != 3) return;
        videoDialog->Hide();
        InitOptionsDialog();
        SetActiveDialog(optionsDialog.get());
    };

    ListDialogConfig listCfg;
    listCfg.labels = labels;
    listCfg.listBoxRect = listBox;
    listCfg.rowHeight = listRow;
    listCfg.scrollButtons = {scrollTop, scrollUp, scrollDown, scrollBottom};
    listCfg.selectionBoxColor = {0xFF, 0xFF, 0xFF, 0xFF};
    listCfg.activateOnClick = true;
    listCfg.onItemSelected = [this](int row) {
        // Fullscreen picks its own scale, so the window size cannot change under it.
        if (video::Fullscreen()) {
            videoDialog->SetSelectedIndex(video::Scale() - 1);
            return;
        }
        video::SetScale(row + 1);
        video::Save();
        rebuildVideo_ = true;
    };
    videoDialog = std::make_unique<ListDialog>(cfg, listCfg);
    videoDialog->SetSelectedIndex(video::Scale() - 1);
}

static const char* EngineSourceLabel(AudioManager::EngineSource src) {
    switch (src) {
        case AudioManager::EngineSource::ADLIB:     return "AdLib";
        case AudioManager::EngineSource::PCSPEAKER: return "PC spkr";
        case AudioManager::EngineSource::C64:       return "C64";
        default:                                    return "Amiga";
    }
}

// Only opened with audio on: AudioManager is null under --mute.
void Garage::InitSoundDialog() {
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (!am) return;

    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int arrowWidth = 20;
    const int arrowHeight = 14;

    // Every soundtrack this install has; the button cycles only when there is more than one.
    std::vector<AudioManager::MusicSource> musicSources;
    if (am->HasAmigaMusic()) musicSources.push_back(AudioManager::MusicSource::AMIGA);
    if (am->HasPCSpeaker())  musicSources.push_back(AudioManager::MusicSource::DOS);
    if (am->HasAdlibMusic()) musicSources.push_back(AudioManager::MusicSource::ADLIB);
    if (am->HasC64Music())   musicSources.push_back(AudioManager::MusicSource::C64);

    SDL_Rect rect = {60, 24, 200, 0};
    const int pad = 5;
    const int innerLeft  = rect.x + pad;
    const int innerRight = rect.x + rect.w - pad;
    const int labelWidth = innerRight - innerLeft;
    const int rowHeight = 18;
    const int fontHeight = SFont_TextHeight(state.GetAssets().GetMainFont());
    const SDL_Color white = {0xFF, 0xFF, 0xFF, 0xFF};

    int y = rect.y + 17;
    auto rowY = [&](int h) { return y + (rowHeight - h) / 2; };

    std::vector<Zone> zones;
    std::vector<DialogTextConfig> texts = {{SDL_Rect{rect.x, rect.y + 3, rect.w, 10}, "Sound", white, true}};
    std::vector<ButtonConfig> buttons;

    auto arrowRow = [&](const std::string& label, const char* upId, const char* upHint, const char* downId,
                        const char* downHint, int upIndex) {
        SDL_Rect up   = {innerRight - arrowWidth, rowY(arrowHeight), arrowWidth, arrowHeight};
        SDL_Rect down = {up.x - arrowWidth - 4, up.y, arrowWidth, arrowHeight};
        texts.push_back({SDL_Rect{innerLeft, rowY(fontHeight), labelWidth, 10}, label, white, false});
        zones.push_back(Zone(upId, upHint, up));
        zones.push_back(Zone(downId, downHint, down));
        buttons.push_back({up,   "[", nullptr, false, upIndex});
        buttons.push_back({down, "]", nullptr, false, upIndex + 1});
        y += rowHeight;
    };
    auto choiceRow = [&](const char* label, const char* id, const char* hint, const char* value, int index) {
        SDL_Rect r = {innerRight - buttonWidth, rowY(buttonHeight), buttonWidth, buttonHeight};
        texts.push_back({SDL_Rect{innerLeft, rowY(fontHeight), labelWidth, 10}, label, white, false});
        zones.push_back(Zone(id, hint, r));
        buttons.push_back({r, value, nullptr, false, index});
        y += rowHeight;
    };

    arrowRow("Sound: " + std::to_string(am->GetSfxVolume() * 10) + "%", "sound_up", "Sound louder",
             "sound_down", "Sound quieter", 0);
    arrowRow("Music: " + std::to_string(am->GetMusicVolume() * 10) + "%", "music_up", "Music louder",
             "music_down", "Music quieter", 2);
    if (!musicSources.empty()) {
        choiceRow("Soundtrack:", "music_source", "Switch music version", MusicSourceLabel(am->GetMusicSource()), 4);
    }
    choiceRow("Engine:", "engine_source", "Amiga sample, Street Rod 2's AdLib, Street Rod's PC speaker or the C64",
              EngineSourceLabel(am->GetEngineSource()), 5);
    choiceRow("Effects:", "sfx_source", "Amiga samples, Street Rod's PC speaker, Street Rod 2's AdLib or the C64",
              am->GetSfxSource() == AudioManager::SfxSource::PCSPEAKER ? "PC spkr"
              : am->GetSfxSource() == AudioManager::SfxSource::ADLIB   ? "AdLib"
              : am->GetSfxSource() == AudioManager::SfxSource::C64     ? "C64"
                                                                        : "Amiga", 7);

    SDL_Rect backRect = {innerRight - buttonWidth, rowY(buttonHeight), buttonWidth, buttonHeight};
    zones.push_back(Zone("back", "Forget it", backRect));
    ButtonConfig backButton = {backRect, "Forget it", nullptr, true, 6};
    backButton.cancelChoice = true;
    buttons.push_back(backButton);
    y += rowHeight;

    rect.h = y - rect.y + pad - 1;

    DialogConfig cfg;
    cfg.bounds          = rect;
    cfg.backgroundColor = {0, 208, 0, 255};
    cfg.borderColor     = {0, 111, 239, 255};
    cfg.texts           = texts;
    cfg.buttons         = buttons;
    cfg.lines           = {{rect.x + 3, rect.y + 12, rect.w - 6, 1}};
    cfg.zones           = zones;
    cfg.contentAreas    = std::vector<SDL_Rect>{rect};
    cfg.font            = state.GetAssets().GetMainFont();
    cfg.onZoneHover     = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onButtonPressed = [this, musicSources](int buttonIndex) {
        AudioManager* am = state.GetAssets().GetAudioManager();
        if (!am) return;
        switch (buttonIndex) {
            case 0: am->SetSfxVolume(am->GetSfxVolume() + 1); break;
            case 1: am->SetSfxVolume(am->GetSfxVolume() - 1); break;
            case 2: am->SetMusicVolume(am->GetMusicVolume() + 1); break;
            case 3: am->SetMusicVolume(am->GetMusicVolume() - 1); break;
            case 4: {
                if (musicSources.size() < 2) return;
                size_t at = 0;
                for (size_t i = 0; i < musicSources.size(); i++) {
                    if (musicSources[i] == am->GetMusicSource()) { at = i; break; }
                }
                am->SetMusicSource(musicSources[(at + 1) % musicSources.size()]);
                break;
            }
            case 5:
                switch (am->GetEngineSource()) {
                    case AudioManager::EngineSource::AMIGA: am->SetEngineSource(AudioManager::EngineSource::ADLIB); break;
                    case AudioManager::EngineSource::ADLIB: am->SetEngineSource(AudioManager::EngineSource::PCSPEAKER); break;
                    case AudioManager::EngineSource::PCSPEAKER:
                        am->SetEngineSource(am->HasC64Engine() ? AudioManager::EngineSource::C64
                                                               : AudioManager::EngineSource::AMIGA);
                        break;
                    default:                                am->SetEngineSource(AudioManager::EngineSource::AMIGA); break;
                }
                break;
            case 7:
                switch (am->GetSfxSource()) {
                    case AudioManager::SfxSource::AMIGA: am->SetSfxSource(AudioManager::SfxSource::PCSPEAKER); break;
                    case AudioManager::SfxSource::PCSPEAKER:
                        am->SetSfxSource(am->HasAdlibSfx() ? AudioManager::SfxSource::ADLIB
                                     : am->HasC64Sfx()     ? AudioManager::SfxSource::C64
                                                           : AudioManager::SfxSource::AMIGA);
                        break;
                    case AudioManager::SfxSource::ADLIB:
                        am->SetSfxSource(am->HasC64Sfx() ? AudioManager::SfxSource::C64
                                                         : AudioManager::SfxSource::AMIGA);
                        break;
                    default: am->SetSfxSource(AudioManager::SfxSource::AMIGA); break;
                }
                break;
            case 6:
                soundDialog->Hide();
                InitOptionsDialog();
                SetActiveDialog(optionsDialog.get());
                return;
        }
        state.GetAssets().SaveSettings();
        rebuildSound_ = true;
    };
    soundDialog = std::make_unique<Dialog>(cfg);
}

void Garage::ShowKeyBindingsDialog() {
    keyBindingsDialog.reset();
    FreeKeyBindingsIcons();
    keyBindingsDialog = MakeKeyBindingsDialog(
        state.GetAssets().GetMainFont(), [this]() {
            keyBindingsDialog->Hide();
            InitOptionsDialog();
            SetActiveDialog(optionsDialog.get());
        });
    SetActiveDialog(keyBindingsDialog.get());
}

void Garage::ShowSettingsDialog() {
    settingsDialog.reset();
    FreeSettingsIcons();
    settingsDialog = MakeSettingsDialog(
        state.GetAssets().GetMainFont(), [this]() {
            settingsDialog->Hide();
            InitOptionsDialog();
            SetActiveDialog(optionsDialog.get());
        });
    SetActiveDialog(settingsDialog.get());
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
