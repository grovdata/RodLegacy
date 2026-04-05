#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"
#include "embedded_loader.h"
#include "audio/audio_manager.h"
#include "engine_part.h"
#include "parsers/libp.h"
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

    Zone saveGameZone(ZONE_GARAGE_MENU_SAVE_GAME, strdup("Save Game"), &saveGameRect);
    Zone oldGameZone(ZONE_GARAGE_MENU_OLD_GAME, strdup("Old Game"), &oldGameRect);
    Zone newGameZone(ZONE_GARAGE_MENU_NEW_GAME, strdup("New Game"), &newGameRect);
    Zone optionsZone(ZONE_GARAGE_MENU_OPTIONS, strdup("Options"), &optionsRect);
    Zone quitGameZone(ZONE_GARAGE_MENU_QUIT_GAME, strdup("Quit"), &quitGameRect);
    Zone forgetItZone(ZONE_GARAGE_MENU_FORGET_IT, strdup("Forget it"), &forgetItRect);

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
        this->hud->SetText(zone.GetText());
    };
    menuDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Save Game");
                // state.SaveGame();
                break;
            case 1:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: Load Game");
                // state.LoadGame();
                break;
            case 2:
                LOG_INFO("Garage::MenuDialogConfig::onButtonPressed: New Game");
                // state.NewGame();
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

    SDL_Rect optionsDialogRect = {60, 50, 200, 120};

    // Button positions
    SDL_Rect soundVolumeUpRect = {190, 70, 20, buttonHeight};
    SDL_Rect soundVolumeDownRect = {165, 70, 20, buttonHeight};
    SDL_Rect musicVolumeUpRect = {190, 90, 20, buttonHeight};
    SDL_Rect musicVolumeDownRect = {165, 90, 20, buttonHeight};
    SDL_Rect fullscreenCheckboxRect = {optionsDialogRect.x + labelWidth + 10, 109, checkboxSize, checkboxSize};
    SDL_Rect backRect = {165, 140, buttonWidth, buttonHeight};

    // Zones
    Zone soundVolumeUpZone(ZONE_GARAGE_OPTIONS_SOUND_UP, strdup("+"), &soundVolumeUpRect);
    Zone soundVolumeDownZone(ZONE_GARAGE_OPTIONS_SOUND_DOWN, strdup("-"), &soundVolumeDownRect);
    Zone musicVolumeUpZone(ZONE_GARAGE_OPTIONS_MUSIC_UP, strdup("+"), &musicVolumeUpRect);
    Zone musicVolumeDownZone(ZONE_GARAGE_OPTIONS_MUSIC_DOWN, strdup("-"), &musicVolumeDownRect);
    Zone fullscreenZone(ZONE_GARAGE_OPTIONS_FULLSCREEN, strdup("Toggle Fullscreen"), &fullscreenCheckboxRect);
    Zone backZone(ZONE_GARAGE_OPTIONS_BACK, strdup("Back"), &backRect);

    std::vector<Zone> dialogZones = {
        soundVolumeUpZone,
        soundVolumeDownZone,
        musicVolumeUpZone,
        musicVolumeDownZone,
        fullscreenZone,
        backZone,
    };

    // Checkbox for fullscreen (added as a ButtonConfig, will be created as Checkbox by Dialog)
    CheckboxConfig fullscreenCheckboxConfig;
    fullscreenCheckboxConfig.bounds = fullscreenCheckboxRect;
    fullscreenCheckboxConfig.value = &fullscreenEnabled;
    fullscreenCheckboxConfig.fillColor = {0, 208, 0, 255};
    fullscreenCheckboxConfig.borderColor = {0, 0, 0, 255};
    fullscreenCheckboxConfig.checkColor = {0, 0, 0, 255};
    fullscreenCheckboxConfig.index = 4;
    fullscreenCheckboxConfig.onChange = [this](bool checked) {
        LOG_INFO("Options: Fullscreen %s", checked ? "enabled" : "disabled");
        // TODO: Toggle fullscreen
        // SDL_SetWindowFullscreen(window, checked ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    };

    // Buttons
    ButtonConfig soundVolumeUpButton = {soundVolumeUpRect, "+", nullptr, false, 0};
    ButtonConfig soundVolumeDownButton = {soundVolumeDownRect, "-", nullptr, false, 1};
    ButtonConfig musicVolumeUpButton = {musicVolumeUpRect, "+", nullptr, false, 2};
    ButtonConfig musicVolumeDownButton = {musicVolumeDownRect, "-", nullptr, false, 3};
    ButtonConfig backButton = {backRect, "Back", nullptr, true, 5};

    AudioManager* am = state.GetAssets().GetAudioManager();
    int sfxVol   = am ? am->GetSfxVolume()   : 8;
    int musicVol = am ? am->GetMusicVolume() : 8;

    char sfxLabel[32], musicLabel[32];
    snprintf(sfxLabel,   sizeof(sfxLabel),   "Sound: %d%%", sfxVol * 10);
    snprintf(musicLabel, sizeof(musicLabel), "Music: %d%%", musicVol * 10);

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{optionsDialogRect.x, optionsDialogRect.y + 3, optionsDialogRect.w, 10}, "Options", {0xFF, 0xFF, 0xFF, 0xFF}, true},
        DialogTextConfig{SDL_Rect{optionsDialogRect.x + 5, 70, labelWidth, 10}, sfxLabel,   {0xFF, 0xFF, 0xFF, 0xFF}, false},
        DialogTextConfig{SDL_Rect{optionsDialogRect.x + 5, 90, labelWidth, 10}, musicLabel, {0xFF, 0xFF, 0xFF, 0xFF}, false},
        DialogTextConfig{SDL_Rect{optionsDialogRect.x + 5, 110, labelWidth, 10}, "Fullscreen:", {0xFF, 0xFF, 0xFF, 0xFF}, false},
    };

    std::vector<SDL_Rect> lines = {
        {63, 62, 194, 1},  // Top line under title
    };

    // Create checkbox manually
    Checkbox* fullscreenCheckbox = new Checkbox(fullscreenCheckboxConfig);

    std::vector<ButtonConfig> buttons = {
        soundVolumeUpButton,
        soundVolumeDownButton,
        musicVolumeUpButton,
        musicVolumeDownButton,
        backButton
    };

    std::vector<Button*> buttonPtrs = {
        fullscreenCheckbox
    };

    DialogConfig optionsDialogConfig;
    optionsDialogConfig.bounds = optionsDialogRect;
    optionsDialogConfig.backgroundColor = {0, 208, 0, 255};
    optionsDialogConfig.borderColor = {0, 111, 239, 255};
    optionsDialogConfig.texts = texts;
    optionsDialogConfig.buttons = buttons;
    optionsDialogConfig.buttonPtrs = buttonPtrs;
    optionsDialogConfig.lines = lines;
    optionsDialogConfig.zones = dialogZones;
    optionsDialogConfig.contentAreas = std::vector<SDL_Rect>{optionsDialogRect};
    optionsDialogConfig.font = state.GetAssets().GetMainFont();
    optionsDialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    optionsDialogConfig.onButtonPressed = [this](int buttonIndex) {
        AudioManager* am = state.GetAssets().GetAudioManager();
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
        }
    };

    optionsDialog = std::make_unique<Dialog>(optionsDialogConfig);
}

void Garage::InitBumperDialogs() {
    // Dialogs are now created dynamically in ShowBumperDialog()
    // This method is kept for compatibility but does nothing
}

