#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"

Garage::Garage() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {

   SDL_Rect bottleRect = {7, 161, 8, 24};
    Zone bottle(ZONE_GARAGE_DRINK, strdup("Wanna drink ?"), &bottleRect);

    SDL_Rect newsPaperRect = {15, 158, 44, 25};
    Zone newsPaper(ZONE_GARAGE_NEWSPAPER, strdup("Check out the newspaper"), &newsPaperRect);

    SDL_Rect changeTiresRect = {136, 157, 26, 29};
    Zone changeTires(ZONE_GARAGE_TIRES, strdup("Change tires"), &changeTiresRect);

    SDL_Rect newPaintJobRect = {228, 160, 33, 24};
    Zone newPaintJob(ZONE_GARAGE_PAINT_JOB, strdup("New paint job - $20"), &newPaintJobRect);

    SDL_Rect gasRect = {288, 161, 23, 26};
    Zone gas(ZONE_GARAGE_GAS_CAN, strdup("Wanna tank ?"), &gasRect);

    SDL_Rect carInfoRect = {290, 107, 27, 14};
    Zone carInfo(ZONE_GARAGE_CAR_INFO, strdup("Car info"), &carInfoRect);

    SDL_Rect calendarRect = {299, 56, 16, 45};
    Zone calendar(ZONE_GARAGE_CALENDAR, strdup("Calendar"), &calendarRect);

    SDL_Rect radioRect = {259, 61, 25, 12};
    Zone radio(ZONE_GARAGE_MUSIC, strdup("Squelsh it !"), &radioRect);

    SDL_Rect stickersRect = {216, 63, 17, 10};
    Zone stickers(ZONE_GARAGE_STICKERS, strdup("Stickers - $5"), &stickersRect);

    SDL_Rect partsRect = {203, 79, 17, 19};
    Zone parts(ZONE_GARAGE_SELL_PARTS, strdup("Sell parts"), &partsRect);

    SDL_Rect menuRect = {156, 65, 33, 14};
    Zone menu(ZONE_GARAGE_MENU, strdup("Time to quit"), &menuRect);

    SDL_Rect carsRect = {69, 41, 200, 16};
    Zone cars(ZONE_GARAGE_CAR_LIST, strdup("Cars"), &carsRect);

    SDL_Rect streetRect = {0, 42, 40, 62};
    Zone street(ZONE_GARAGE_GOTO_DINER, strdup("Hit the street"), &streetRect);

    SDL_Rect rearBumperRect = {290, 107, 27, 14};
    Zone rearBumper(ZONE_GARAGE_REAR_BUMPER, strdup("Rear bumper"), &rearBumperRect);

    SDL_Rect frontBumperRect = {290, 107, 27, 14};
    Zone frontBumper(ZONE_GARAGE_FRONT_BUMPER, strdup("Front bumper"), &frontBumperRect);

    SDL_Rect roofRect = {290, 107, 27, 14};
    Zone roof(ZONE_GARAGE_CHOPPED_ROOF, strdup("Roof"), &roofRect);

    SDL_Rect transmissionRect = {290, 107, 27, 14};
    Zone transmission(ZONE_GARAGE_TRANSMISSION, strdup("Transmission"), &transmissionRect);

    SDL_Rect engineRect = {290, 107, 27, 14};
    Zone engine(ZONE_GARAGE_ENGINE, strdup("Pop the hood"), &engineRect);

    signRect = {72, 44, 195, 12};

    zones.push_back(bottle);
    zones.push_back(newsPaper);
    zones.push_back(changeTires);
    zones.push_back(newPaintJob);
    zones.push_back(gas);
    zones.push_back(carInfo);
    zones.push_back(calendar);
    zones.push_back(radio);
    zones.push_back(stickers);
    zones.push_back(parts);
    zones.push_back(menu);
    zones.push_back(cars);
    zones.push_back(street);
    zones.push_back(rearBumper);
    zones.push_back(frontBumper);
    zones.push_back(roof);
    zones.push_back(transmission);
    zones.push_back(engine);

    Bubble *bubbleDrink = new Bubble("  Don\'t drink and drive, bub!", BUBBLE_LEFT, 103, 30, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(bubbleDrink);
    Bubble *partsBubble = new Bubble("  You\'ve got no spare parts, dummy!", BUBBLE_LEFT, 93, 30, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(partsBubble);
    Bubble *tiresBubble = new Bubble("  You\'ve got no spare tires, dummy!", BUBBLE_LEFT, 79, 30, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(tiresBubble);

    currentPointer = SrLibAmigaSpriteAssetMousePointerWrench;

    signSurface = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(signSurface, SDL_BLENDMODE_BLEND);

    // Clear the entire sign area initially
    SDL_FillRect(signSurface, NULL, SDL_MapRGBA(signSurface->format, 0, 0, 0, 0));

    // Create initial texture
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);

    // Set audio manager for engine sound
    SetAudioManager(state.assets.GetAudioManager());

    // Don't start animation here - it will be started after initialDelay in Draw()

    std::vector<std::string> labels;
    for (const auto& car : state.GetOwnedCars()) {
        labels.push_back(car->GetModel().name);
    }
    
    SDL_Rect bounds = {36, 26, 260, 136};

    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int buttonY = bounds.y + bounds.h - buttonHeight - 8;
    
    SDL_Rect listBoxRect = {40, 40, 242, 80};    
    SDL_Rect topRect = {37, 27, 246, 95};
    SDL_Rect bottomRect = {37, 123, 246, 40};
    SDL_Rect selectedItemRect = {39, 127, 242, 12};
    SDL_Rect switchItRect = {bounds.x + 14, buttonY, buttonWidth, buttonHeight};
    SDL_Rect sellItRect = {bounds.x + 90, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {bounds.x + 165, buttonY, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};     // top
    SDL_Rect scrollUpRect = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};     // up
    SDL_Rect scrollDownRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15}; // down
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};  // bottom
    
    // Dialog zones
    Zone switchItZone(ZONE_GARAGE_BUTTON_SWITCH_IT, strdup("Switch it"), &switchItRect);
    Zone sellItZone(ZONE_GARAGE_BUTTON_SELL_IT, strdup("Sell it"), &sellItRect);
    Zone forgetItZone(ZONE_GARAGE_BUTTON_FORGET_IT, strdup("Forget it"), &forgetItRect);
    Zone scrollTopZone(ZONE_GARAGE_BUTTON_SCROLL_TOP, strdup("\\"), &scrollTopRect);
    Zone scrollDownZone(ZONE_GARAGE_BUTTON_SCROLL_DOWN, strdup(">"), &scrollDownRect);
    Zone scrollUpZone(ZONE_GARAGE_BUTTON_SCROLL_UP, strdup("<"), &scrollUpRect);
    Zone scrollBottomZone(ZONE_GARAGE_BUTTON_SCROLL_BOTTOM, strdup("/"), &scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {
            switchItRect,
            "Switch it",
            nullptr,  // onClick will be set by Dialog
            true,     // defaultChoice
            0        // index            
        },
        {
            sellItRect,
            "Sell it",
            nullptr,
            false,
            1
        },
        {
            forgetItRect,
            "Forget it",
            nullptr,
            false,
            2
        }
    };

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Your cars", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {40, 39, 240, 1},
    };

    // Add dialog zones to config
    std::vector<Zone> dialogZones = {
        switchItZone,
        sellItZone,
        forgetItZone,
        scrollTopZone,
        scrollDownZone,
        scrollUpZone,
        scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;  // Add the dialog zones here
    dialogConfig.contentAreas = std::vector<SDL_Rect>{topRect, bottomRect};
    dialogConfig.font = state.assets.GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Switch it
                int selectedIndex = carSelectDialog->GetSelectedIndex();
                if (selectedIndex >= 0) {
                    Car* selectedCar = state.GetOwnedCars()[selectedIndex];
                    if (selectedCar != state.GetCurrentCar()) {
                        pendingCar = selectedCar;
                        StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                    }
                }
                carSelectDialog->Hide();
                break;
            }
            case 1: // Sell it
                carSelectDialog->Hide();
                break;
            case 2: // Forget it
                carSelectDialog->Hide();
                break;
        }
    };
    dialogConfig.texts = texts;

    ListDialogConfig listDialogConfig;
    listDialogConfig.labels = labels;
    listDialogConfig.listBoxRect = listBoxRect;
    listDialogConfig.selectedItemRect = selectedItemRect;

    carSelectDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            [this](int selectedIndex) {
                Car* selectedCar = state.GetOwnedCars()[selectedIndex];
            SDL_Log("Garage::ListDialog: Selected index=%d, car=%s (ptr: %p)", 
                   selectedIndex, 
                   selectedCar->GetModel().name.c_str(), 
                   (void*)selectedCar);
            
            // Only switch if selecting a different car
            if (selectedCar != state.GetCurrentCar()) {
                pendingCar = selectedCar;
                StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                SDL_Log("Garage::ListDialog: Switching from current car (ptr: %p) to pending car (ptr: %p)",
                       (void*)state.GetCurrentCar(),
                       (void*)pendingCar);
            } else {
                SDL_Log("Garage::ListDialog: Same car selected, ignoring");
            }
        },
        listBoxRect,
        selectedItemRect,
        {0xFF, 0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0x00, 0xFF},
        {
            scrollTopRect,
            scrollUpRect,
            scrollDownRect,
            scrollBottomRect            
        }
    });

    InitGameMenuDialog();
    InitOptionsDialog();
    InitBumperDialogs();

    engineBay = std::make_unique<EngineBay>(
        state.GetCurrentCar()->GetTuneLevel(),
        state,
        [this](int buttonIndex) {
            switch (buttonIndex) {
                case 0: // Tune
                    if (engineBay->GetCurrentState() == ENGINE_BAY_OVERVIEW) {
                        engineBay->SwitchToState(ENGINE_BAY_TUNING);
                        SetActiveDialog(engineBay->GetDialog());
                    } else {
                        engineBay->SwitchToState(ENGINE_BAY_OVERVIEW);
                        SetActiveDialog(engineBay->GetDialog());
                    }
                    break;
                case 1: // Parts
                    engineBay->SwitchToState(ENGINE_BAY_PARTS);
                    SetActiveDialog(engineBay->GetDialog());
                    break;
                case 2: // Done
                    //state.SetGameState(GAME_STATE_GARAGE);
                    engineBay->GetDialog()->Hide();
                    SetActiveDialog(nullptr);
                    break;
                case 99: // Special case: just update dialog (used by tuning Done button)
                    SetActiveDialog(engineBay->GetDialog());
                    break;
            }
        },
        [this](Zone& zone) {
            this->hud->SetText(zone.GetText());
        }
    );
    
    InitCarBitmaps();
    
    // Set initial car bitmap
    if (state.GetCurrentCar()) {
        carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    }
}

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
    menuDialogConfig.font = state.assets.GetMainFont();
    menuDialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    menuDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                SDL_Log("Garage::MenuDialogConfig::onButtonPressed: Save Game");
                // state.SaveGame();
                break;
            case 1:
                SDL_Log("Garage::MenuDialogConfig::onButtonPressed: Load Game");
                // state.LoadGame();
                break;
            case 2:
                SDL_Log("Garage::MenuDialogConfig::onButtonPressed: New Game");
                // state.NewGame();
                break;
            case 3: {
                SDL_Log("Garage::MenuDialogConfig::onButtonPressed: Quit Game");
                SDL_Event event = {};
                event.type = SDL_QUIT;
                SDL_PushEvent(&event);
                break;
            }
            case 4:
                gameMenuDialog->Hide();
                break;
            case 5:
                SDL_Log("Garage::MenuDialogConfig::onButtonPressed: Options");
                gameMenuDialog->Hide();
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
        SDL_Log("Options: Fullscreen %s", checked ? "enabled" : "disabled");
        // TODO: Toggle fullscreen
        // SDL_SetWindowFullscreen(window, checked ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    };

    // Buttons
    ButtonConfig soundVolumeUpButton = {soundVolumeUpRect, "+", nullptr, false, 0};
    ButtonConfig soundVolumeDownButton = {soundVolumeDownRect, "-", nullptr, false, 1};
    ButtonConfig musicVolumeUpButton = {musicVolumeUpRect, "+", nullptr, false, 2};
    ButtonConfig musicVolumeDownButton = {musicVolumeDownRect, "-", nullptr, false, 3};
    ButtonConfig backButton = {backRect, "Back", nullptr, true, 5};

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{optionsDialogRect.x, optionsDialogRect.y + 3, optionsDialogRect.w, 10}, "Options", {0xFF, 0xFF, 0xFF, 0xFF}, true},
        DialogTextConfig{SDL_Rect{optionsDialogRect.x + 5, 70, labelWidth, 10}, "Sound Volume:", {0xFF, 0xFF, 0xFF, 0xFF}, false},
        DialogTextConfig{SDL_Rect{optionsDialogRect.x + 5, 90, labelWidth, 10}, "Music Volume:", {0xFF, 0xFF, 0xFF, 0xFF}, false},
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
    optionsDialogConfig.font = state.assets.GetMainFont();
    optionsDialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    optionsDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                SDL_Log("Options: Sound Volume Up");
                // TODO: Increase sound volume
                // state.audio.IncreaseSoundVolume();
                break;
            case 1:
                SDL_Log("Options: Sound Volume Down");
                // TODO: Decrease sound volume
                // state.audio.DecreaseSoundVolume();
                break;
            case 2:
                SDL_Log("Options: Music Volume Up");
                // TODO: Increase music volume
                // state.audio.IncreaseMusicVolume();
                break;
            case 3:
                SDL_Log("Options: Music Volume Down");
                // TODO: Decrease music volume
                // state.audio.DecreaseMusicVolume();
                break;
            case 5:
                SDL_Log("Options: Back to menu");
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

void Garage::ShowBumperDialog(bool isFront) {
    // Check current bumper status
    bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
    const char* titleText = isFront ? (isStripped ? "Restore bumper - $20" : "Strip bumper - $20") 
                                    : (isStripped ? "Restore bumper - $20" : "Strip bumper - $20");
    
    std::unique_ptr<Dialog>& dialogPtr = isFront ? frontBumperDialog : rearBumperDialog;
    
    auto onConfirm = [this, isFront, isStripped, &dialogPtr]() {
        if (carBitmap) {
            if (isStripped) {
                // Restore
                if (isFront) {
                    carBitmap->RestoreFrontBumper();
                    SDL_Log("Front bumper restored for $20");
                } else {
                    carBitmap->RestoreRearBumper();
                    SDL_Log("Rear bumper restored for $20");
                }
            } else {
                // Strip
                if (isFront) {
                    carBitmap->StripFrontBumper();
                    SDL_Log("Front bumper stripped for $20");
                } else {
                    carBitmap->StripRearBumper();
                    SDL_Log("Rear bumper stripped for $20");
                }
            }
        }
        
        dialogPtr->Hide();
        SetActiveDialog(nullptr);
    };
    
    ShowModificationDialog(titleText, 20, onConfirm, dialogPtr);
}

void Garage::ShowRoofDialog() {
    // Check current roof status
    bool isChopped = carBitmap->IsRoofChopped();
    const char* titleText = isChopped ? "Restore roof - $70" : "Chop roof - $70";
    
    auto onConfirm = [this, isChopped]() {
        if (carBitmap) {
            if (isChopped) {
                // Restore
                carBitmap->RestoreRoof();
                SDL_Log("Roof restored for $70");
            } else {
                // Chop
                carBitmap->ChopRoof();
                SDL_Log("Roof chopped for $70");
            }
        }
        
        roofDialog->Hide();
        SetActiveDialog(nullptr);
    };
    
    ShowModificationDialog(titleText, 70, onConfirm, roofDialog);
}

void Garage::ShowModificationDialog(const char* title, int price, std::function<void()> onConfirm, std::unique_ptr<Dialog>& dialogPtr) {
    // Dialog dimensions as specified
    const int dialogWidth = 156;
    const int dialogHeight = 36;
    const int dialogX = 82;
    const int dialogY = 50;
    
    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};
    
    // Check if player has enough money
    int currentMoney = state.GetBankroll();
    bool canAfford = currentMoney >= price;
    
    // Button layout
    const int buttonWidth = 70;
    const int buttonHeight = 14;
    const int buttonSpacing = 4;
    const int buttonY = dialogY + dialogHeight - buttonHeight - 4;
    
    SDL_Rect okButtonRect = {dialogX + 4, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetButtonRect = {dialogX + 4 + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight};
    
    ButtonConfig okButton = {okButtonRect, "OK", nullptr, true, 0}; // OK is default
    ButtonConfig forgetButton = {forgetButtonRect, "Forget it", nullptr, false, 1};
    
    std::vector<ButtonConfig> buttons = {okButton, forgetButton};
    
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{dialogX + 3, dialogY + 3, dialogWidth, 10}, title, {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    
    std::vector<SDL_Rect> lines = {
        {dialogX + 3, dialogY + 14, dialogWidth - 6, 1}
    };
    
    DialogConfig modDialogConfig;
    modDialogConfig.bounds = dialogRect;
    modDialogConfig.backgroundColor = {0, 208, 0, 255};
    modDialogConfig.borderColor = {0, 111, 239, 255};
    modDialogConfig.texts = texts;
    modDialogConfig.buttons = buttons;
    modDialogConfig.lines = lines;
    modDialogConfig.contentAreas = std::vector<SDL_Rect>{dialogRect};
    modDialogConfig.font = state.assets.GetMainFont();
    modDialogConfig.onButtonPressed = [this, price, canAfford, onConfirm, &dialogPtr](int index) {
        switch (index) {
            case 0: // OK
                if (!canAfford) {
                    SDL_Log("Not enough money! Need $%d", price);
                    dialogPtr->Hide();
                    SetActiveDialog(nullptr);
                    return;
                }
                
                // Deduct price
                state.SetBankroll(state.GetBankroll() - price);
                
                // Execute the modification
                onConfirm();
                break;
            case 1: // Forget it
                dialogPtr->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    
    // Create and show the dialog
    dialogPtr = std::make_unique<Dialog>(modDialogConfig);
    SetActiveDialog(dialogPtr.get());
}

SDL_Point Garage::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GARAGE);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, state.GetCurrentCar()->GetModel().index);
    return {x, y};
}

void Garage::UpdateSign() {

    SDL_Log("Garage::UpdateSign");
    char text[50];
    CarModel m = state.GetCurrentCar()->GetModel();
    snprintf(text, 50, "%s.", m.name.c_str());
    
    // Clear the surface
    Uint32 transparent = SDL_MapRGBA(signSurface->format, 0, 0, 0, 0);
    SDL_FillRect(signSurface, NULL, transparent);
    
    // Write new text
    int textWidth = SFont_TextWidth(state.assets.GetMainFont(), text);
    int x = signRect.x + (signRect.w - textWidth) / 2;
    SFont_Write(signSurface, state.assets.GetMainFont(), x, 45, text);
    
    // Destroy old texture if it exists
    if (signTexture) {
        SDL_DestroyTexture(signTexture);
    }
    
    // Create new texture
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
}

void Garage::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    switch (zone)
    {
    case ZONE_GARAGE_GOTO_DINER:
        state.MoveToScene(SCENE_DINER);
        break;
    case ZONE_GARAGE_NEWSPAPER:
        state.MoveToScene(SCENE_NEWSPAPER);
        break;  
    case ZONE_GARAGE_GAS_CAN:
        state.MoveToScene(SCENE_GAS_STATION);
        break;
    case ZONE_GARAGE_DRINK:
        bubbles[0]->SetVisible(true);
        break;
    case ZONE_GARAGE_SELL_PARTS:
        bubbles[1]->SetVisible(true);
        break;
    case ZONE_GARAGE_TIRES:
        bubbles[2]->SetVisible(true);
        break;
    case ZONE_GARAGE_CAR_LIST:
        SetActiveDialog((Dialog*)carSelectDialog.get());
        break;
    case ZONE_GARAGE_MENU:
        SetActiveDialog((Dialog*)gameMenuDialog.get());        
        break;
    case ZONE_GARAGE_ENGINE:
        engineBay->ResetToOverview();
        SetActiveDialog((Dialog*)engineBay->GetDialog());
        break;
    case ZONE_GARAGE_FRONT_BUMPER:
        ShowBumperDialog(true);  // true = front bumper
        break;
    case ZONE_GARAGE_REAR_BUMPER:
        ShowBumperDialog(false);  // false = rear bumper
        break;
    case ZONE_GARAGE_CHOPPED_ROOF:
        ShowRoofDialog();
        break;
    default:
        break;
    }
}

void Garage::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    Scene::Draw();

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            needsNewAnimation = true;  // Ensure parent's initialization logic runs
            
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
        return;
    }

    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();

        // get clickboxes
        CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;
        
        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect(ZONE_GARAGE_REAR_BUMPER, &rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect(ZONE_GARAGE_CHOPPED_ROOF, &choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect(ZONE_GARAGE_FRONT_BUMPER, &frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect(ZONE_GARAGE_TRANSMISSION, &transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect(ZONE_GARAGE_ENGINE, &engineRect);
    }
    

    DrawDebugZones();
    
    SDL_RenderCopy(renderer, signTexture, NULL, NULL);

    for (auto b : bubbles) {
        b->Draw();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }
}

void Garage::OnFadeComplete() {
    CarScene::OnFadeComplete();
}

void Garage::SetActiveDialog(Dialog *dialog) {
    // Call the base class implementation
    CarScene::SetActiveDialog(dialog);
    
    // If this is the engine bay dialog, restore the correct visibility
    if (dialog == engineBay->GetDialog()) {
        engineBay->OnDialogShown();
    }
}

void Garage::PushEvent(SDL_Event* event) {
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return;  // Skip normal zone handling when dialog is visible
    }

    CarScene::PushEvent(event);
}

void Garage::OnCarAnimationComplete() {
    SDL_Log("Garage::OnCarAnimationComplete - Entry point");
    CarScene::OnCarAnimationComplete();
    if (pendingCar != nullptr) {
        state.SetCurrentCar(pendingCar);
        
        UpdateSign();
        
        // Mark that we need a new animation (important for parent's StartAnimateCar logic)
        needsNewAnimation = true;
        
        // Start the drive-in animation for the new car (this will set up audio, bitmap, position, etc.)
        SDL_Log("Garage::OnCarAnimationComplete - Starting drive-in animation for new car");
        StartAnimateCar(pendingCar, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
        
        pendingCar = nullptr;
    }
}

void Garage::InitCarBitmaps() {
    // Create bitmaps for all owned cars
    for (auto& car : state.GetOwnedCars()) {
        if (carBitmaps.find(car) == carBitmaps.end()) {
            carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
        }
    }
}

CarBitmap* Garage::GetOrCreateCarBitmap(Car* car) {
    auto it = carBitmaps.find(car);
    if (it == carBitmaps.end()) {
        carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
    }
    return carBitmaps[car].get();
}

Garage::~Garage() {
    carBitmaps.clear();
}

void Garage::StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;
    
    SDL_Log("Garage::StartAnimateCar - car=%p, type=%d", (void*)car, animType);
    
    // Garage-specific setup BEFORE calling parent
    // Create/get car bitmap
    carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), state.GetCurrentCar()->GetModel().index);
    
    // Set initial position based on animation type
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
        // Reverse: start at idle position (car is already visible)
        SDL_Point idlePos = GetCarIdlePosition();
        carx = idlePos.x;
        SDL_Log("Garage::StartAnimateCar - Reverse animation, car at idle x=%d", carx);
    } else {
        // Drive in: start off-screen left
        carx = -GAME_WIDTH;
        SDL_Log("Garage::StartAnimateCar - Drive in animation, car off-screen x=%d", carx);
    }
    
    carBitmap->Move({carx, cary});
    
    // Call parent method to start engine sound and set up animation
    // Parent will NOT override our position for reverse animations
    CarScene::StartAnimateCar(car, driver, animType);
    
    // Update sign for initial car
    UpdateSign();
    
    currentAnimationType = animType;
    SDL_Log("Garage::StartAnimateCar - Animation started");
}