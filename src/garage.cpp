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

    auto bubbleDrink = std::make_unique<Bubble>("  Don\'t drink and drive, bub!", BUBBLE_LEFT, 103, 30, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(std::move(bubbleDrink));
    auto partsBubble = std::make_unique<Bubble>("  You\'ve got no spare parts, dummy!", BUBBLE_LEFT, 93, 30, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(std::move(partsBubble));
    auto tiresBubble = std::make_unique<Bubble>("  You\'ve got no spare tires, dummy!", BUBBLE_LEFT, 79, 30, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(std::move(tiresBubble));
    auto paintBubble = std::make_unique<Bubble>("  Paint ain\'t free!", BUBBLE_RIGHT, 103, 30, true);
    paintBubble->SetVisible(false);
    bubbles.push_back(std::move(paintBubble));
    auto customizationBubble = std::make_unique<Bubble>("  Customization ain\'t free!", BUBBLE_RIGHT, 103, 30, true);
    customizationBubble->SetVisible(false);
    bubbles.push_back(std::move(customizationBubble));
    auto roofConstraintBubble = std::make_unique<Bubble>("  This car can\'t be chopped!", BUBBLE_RIGHT, 103, 30, true);
    roofConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(roofConstraintBubble));
    auto bumperConstraintBubble = std::make_unique<Bubble>("  This car can\'t be stripped!", BUBBLE_RIGHT, 103, 30, true);
    bumperConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(bumperConstraintBubble));

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
    SetAudioManager(state.GetAssets().GetAudioManager());

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
    dialogConfig.font = state.GetAssets().GetMainFont();
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
            LOG_INFO("Garage::ListDialog: Selected index=%d, car=%s (ptr: %p)", 
                   selectedIndex, 
                   selectedCar->GetModel().name.c_str(), 
                   (void*)selectedCar);
            
            // Update paint can to show the selected car's color
            previewColor = selectedCar->GetColor();
            originalColor = previewColor;
            LOG_INFO("Car list: Updating paint can to color %d for selected car", (int)previewColor);
            UpdatePaintCanPreview();
            
            // Only switch if selecting a different car
            if (selectedCar != state.GetCurrentCar()) {
                pendingCar = selectedCar;
                carSelectDialog->Hide();  // Hide dialog immediately when switching cars
                StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                LOG_INFO("Garage::ListDialog: Switching from current car (ptr: %p) to pending car (ptr: %p)",
                       (void*)state.GetCurrentCar(),
                       (void*)pendingCar);
            } else {
                LOG_INFO("Garage::ListDialog: Same car selected, ignoring");
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

    // Initialize paint color tracking to current car's color
    StandardCarColor currentCarColor = state.GetCurrentCar() ? state.GetCurrentCar()->GetColor() : CarColorRed;
    previewColor = currentCarColor;
    originalColor = currentCarColor;
    LOG_INFO("Garage constructor: Initializing paint can to current car color %d", (int)currentCarColor);
    
    // Initialize paint can preview (always visible)
    paintCanPreview = std::make_unique<Bitmap>(10, 12, 2);
    paintCanPreview->SetPosition(233, 166);
    paintCanPreview->SetVisible(true);
    UpdatePaintCanPreview();  // Set initial color
    
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
    optionsDialogConfig.font = state.GetAssets().GetMainFont();
    optionsDialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    optionsDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                LOG_INFO("Options: Sound Volume Up");
                // TODO: Increase sound volume
                // state.audio.IncreaseSoundVolume();
                break;
            case 1:
                LOG_INFO("Options: Sound Volume Down");
                // TODO: Decrease sound volume
                // state.audio.DecreaseSoundVolume();
                break;
            case 2:
                LOG_INFO("Options: Music Volume Up");
                // TODO: Increase music volume
                // state.audio.IncreaseMusicVolume();
                break;
            case 3:
                LOG_INFO("Options: Music Volume Down");
                // TODO: Decrease music volume
                // state.audio.DecreaseMusicVolume();
                break;
            case 5:
                LOG_INFO("Options: Back to menu");
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
    // Hide any previous error bubble
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }
    
    // Check if car can strip bumpers
    if (state.GetCurrentCar()) {
        CarModel model = state.GetCurrentCar()->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
        
        // If trying to strip bumpers, check if car supports it
        bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
        if (!isStripped && !CarQuery::SrLibQueryCanStripBumpers(bodyAssetNo)) {
            // Show constraint bubble instead of dialog
            if (bubbles.size() > 6) {
                bubbles[6]->SetVisible(true);  // bumperConstraintBubble
            }
            return;
        }
    }
    
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
                    LOG_INFO("Front bumper restored for $20");
                } else {
                    carBitmap->RestoreRearBumper();
                    LOG_INFO("Rear bumper restored for $20");
                }
                // Play bash sound effect
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
                }
            } else {
                // Strip
                if (isFront) {
                    carBitmap->StripFrontBumper();
                    LOG_INFO("Front bumper stripped for $20");
                } else {
                    carBitmap->StripRearBumper();
                    LOG_INFO("Rear bumper stripped for $20");
                }
                // Play bash sound effect
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
                }
            }
        }
        
        dialogPtr->Hide();
        SetActiveDialog(nullptr);
    };
    
    ShowModificationDialog(titleText, 20, onConfirm, dialogPtr);
}

void Garage::ShowRoofDialog() {
    // Hide any previous error bubble
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }
    
    // Check if car can chop roof
    if (state.GetCurrentCar()) {
        CarModel model = state.GetCurrentCar()->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
        
        // If trying to chop roof, check if car supports it
        bool isChopped = carBitmap->IsRoofChopped();
        if (!isChopped && !CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
            // Show constraint bubble instead of dialog
            if (bubbles.size() > 5) {
                bubbles[5]->SetVisible(true);  // roofConstraintBubble
            }
            return;
        }
    }
    
    // Check current roof status
    bool isChopped = carBitmap->IsRoofChopped();
    const char* titleText = isChopped ? "Restore roof - $70" : "Chop roof - $70";
    
    auto onConfirm = [this, isChopped]() {
        if (carBitmap) {
            if (isChopped) {
                // Restore
                carBitmap->RestoreRoof();
                LOG_INFO("Roof restored for $70");
                // Play bash sound effect
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
                }
            } else {
                // Chop
                carBitmap->ChopRoof();
                LOG_INFO("Roof chopped for $70");
                // Play bash sound effect
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
                }
            }
        }
        
        roofDialog->Hide();
        SetActiveDialog(nullptr);
    };
    
    ShowModificationDialog(titleText, 70, onConfirm, roofDialog);
}

void Garage::CycleNextColor() {
    int nextColor = (previewColor + 1) % CarColorCount;
    previewColor = static_cast<StandardCarColor>(nextColor);
    UpdatePaintCanPreview();
    // Hide any error bubble when cycling colors
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::CyclePreviousColor() {
    int prevColor = (previewColor - 1 + CarColorCount) % CarColorCount;
    previewColor = static_cast<StandardCarColor>(prevColor);
    UpdatePaintCanPreview();
    // Hide any error bubble when cycling colors
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::ApplyPreviewColor() {
    if (carBitmap) {
        carBitmap->UpdateColor(previewColor);
        // Update paint can to show the new current color
        UpdatePaintCanPreview();
    }
}

void Garage::UpdatePaintCanPreview() {
    if (!paintCanPreview) return;
    
    // Load the paint can mask image from embedded resource
    extern const unsigned char embedded_paintcan_png[];
    extern const size_t embedded_paintcan_png_len;
    
    SDL_Surface* maskSurface = LoadSurfaceFromMemory(embedded_paintcan_png, embedded_paintcan_png_len);
    if (!maskSurface) {
        LOG_INFO("Failed to load embedded paintcan.png");
        return;
    }
    
    // Get the preview color palette
    std::vector<uint16_t> colorPalette = GetStandardCarColor(previewColor);
    
    // Convert the mask surface to 32-bit RGBA
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(maskSurface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(maskSurface);
    
    if (!rgbaSurface) {
        LOG_INFO("Failed to convert paintcan surface");
        return;
    }
    
    // Apply the car color palette to the paint can
    // The mask should have grayscale values that we'll colorize
    SDL_LockSurface(rgbaSurface);
    uint32_t* pixels = static_cast<uint32_t*>(rgbaSurface->pixels);
    
    for (int y = 0; y < rgbaSurface->h; y++) {
        for (int x = 0; x < rgbaSurface->w; x++) {
            int idx = y * rgbaSurface->w + x;
            uint32_t pixel = pixels[idx];
            
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, rgbaSurface->format, &r, &g, &b, &a);
            
            // If the pixel is not transparent, apply the car color
            if (a > 0) {
                // Use the grayscale value to pick a shade from the palette
                int brightness = (r + g + b) / 3;
                int paletteIndex = (brightness * (colorPalette.size() - 1)) / 255;
                if (paletteIndex >= colorPalette.size()) paletteIndex = colorPalette.size() - 1;
                
                uint16_t amigaColor = colorPalette[paletteIndex];
                
                // Convert Amiga color to RGB
                uint8_t newR = ((amigaColor >> 8) & 0x0F) * 17;
                uint8_t newG = ((amigaColor >> 4) & 0x0F) * 17;
                uint8_t newB = (amigaColor & 0x0F) * 17;
                
                pixels[idx] = SDL_MapRGBA(rgbaSurface->format, newR, newG, newB, a);
            }
        }
    }
    
    SDL_UnlockSurface(rgbaSurface);
    
    // Update the bitmap with the new surface
    paintCanPreview->ReplaceSurface(rgbaSurface);
}

void Garage::ShowPaintDialog() {
    // Store the original color so we can restore it if user cancels
    if (carBitmap) {
        originalColor = carBitmap->GetCurrentColor();
        previewColor = originalColor;
    }
    
    // Hide any previous error bubble
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
    
    // Update paint can to show preview color
    UpdatePaintCanPreview();
    
    // Button positions as specified
    SDL_Rect nextColorRect = {149, 127, 78, 16};
    SDL_Rect prevColorRect = {233, 127, 78, 16};
    SDL_Rect goAheadRect = {149, 145, 78, 16};
    SDL_Rect forgetItRect = {233, 145, 78, 16};
    
    ButtonConfig nextColorButton = {nextColorRect, "Next color", nullptr, false, 0};
    ButtonConfig prevColorButton = {prevColorRect, "Previous color", nullptr, false, 1};
    ButtonConfig goAheadButton = {goAheadRect, "Go ahead", nullptr, true, 2}; // Default
    ButtonConfig forgetItButton = {forgetItRect, "Forget it", nullptr, false, 3};
    
    std::vector<ButtonConfig> buttons = {nextColorButton, prevColorButton, goAheadButton, forgetItButton};
    
    DialogConfig paintDialogConfig;
    paintDialogConfig.bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT}; // Full screen but transparent
    paintDialogConfig.backgroundColor = {0, 0, 0, 0}; // Transparent
    paintDialogConfig.borderColor = {0, 111, 239, 255};
    paintDialogConfig.buttons = buttons;
    paintDialogConfig.contentAreas = {}; // No content areas = no background drawn
    paintDialogConfig.font = state.GetAssets().GetMainFont();
    paintDialogConfig.onButtonPressed = [this](int index) {
        switch (index) {
            case 0: // Next color
                CycleNextColor();
                break;
            case 1: // Previous color
                CyclePreviousColor();
                break;
            case 2: // Go ahead
                {
                    int currentMoney = state.GetBankroll();
                    if (currentMoney >= 20) {
                        state.SetBankroll(currentMoney - 20);
                        Car* car = state.GetCurrentCar();
                        if (car) {
                            car->SetColor(previewColor);
                            car->SetDecal(SrLibCarDecalNone);  // Remove sticker when repainted
                        }
                        ApplyPreviewColor();
                        if (car) {
                            GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                        }
                        LOG_INFO("Car painted for $20");
                        if (bubbles.size() > 3) {
                            bubbles[3]->SetVisible(false);  // Hide paintBubble
                        }
                        paintDialog->Hide();
                        SetActiveDialog(nullptr);
                    } else {
                        // Show "Paint ain't free!" bubble and keep dialog open
                        LOG_INFO("Not enough money! Need $20");
                        if (bubbles.size() > 3) {
                            bubbles[3]->SetVisible(true);  // paintBubble
                        }
                    }
                }
                break;
            case 3: // Forget it
                // Just close - restore original color on paint can
                previewColor = originalColor;
                UpdatePaintCanPreview();
                if (bubbles.size() > 3) {
                    bubbles[3]->SetVisible(false);  // Hide paintBubble
                }
                paintDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    
    // Create and show the dialog
    paintDialog = std::make_unique<Dialog>(paintDialogConfig);
    SetActiveDialog(paintDialog.get());
}

void Garage::ShowCarInfoDialog() {
    // Create dialog bounds
    SDL_Rect dialogRect = {37, 27, 246, 116};
    
    std::vector<DialogTextConfig> texts;
    // Title centered
    texts.push_back(DialogTextConfig{SDL_Rect{dialogRect.x + 3, dialogRect.y + 3, dialogRect.w - 6, 10}, "Car parts", {0xFF, 0xFF, 0xFF, 0xFF}, true});
    
    // Horizontal line under title
    std::vector<SDL_Rect> lines = {
        {dialogRect.x + 3, dialogRect.y + 14, dialogRect.w - 6, 1}
    };
    
    // Content text area start Y
    int y = dialogRect.y + 20;
    int xLeft = dialogRect.x + 8;
    int xRightAlign = dialogRect.x + dialogRect.w - 12; // right margin for right-aligned notes
    
    auto addLeft = [&](const char* s) {
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft, y, dialogRect.w - 16, 10}, s, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };
    auto addIndented = [&](const char* s) {
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft + 10, y, dialogRect.w - 26, 10}, s, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };
    auto addRightNote = [&](const char* left, const char* note) {
        // Render left, then compute right-aligned note position using measured width
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft, y, dialogRect.w - 16, 10}, left, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        int noteWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), note);
        texts.push_back(DialogTextConfig{SDL_Rect{xRightAlign - noteWidth, y, noteWidth, 10}, note, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };
    
    // Populate from current car parts (engine, manifold, carb, transmission, tires)
    Car* car = state.GetCurrentCar();
    if (car) {
        CarModel model = car->GetModel();
        auto vendorToStr = [](CarVendor_t v) -> const char* {
            switch (v) {
                case CarVendorGM: return "GM";
                case CarVendorFord: return "Ford";
                case CarVendorChrysler: return "Chrysler";
                case CarVendorCustom: return "Custom";
                default: return "Unknown";
            }
        };
        auto wearNote = [](int wear) {
            if (wear <= 0) return std::string("(new)");
            char buf[32];
            snprintf(buf, sizeof(buf), "(%d%% worn)", wear);
            return std::string(buf);
        };
        auto transName = [vendorToStr](AmigaLibAsset aid, CarVendor_t vendor) -> std::string {
            switch ((SrTransmissionType)aid) {
                case SR_TRANS_AUTO_GM: return "Auto Trnsmsn GM.";
                case SR_TRANS_AUTO_FORD: return "Auto Trnsmsn Ford.";
                case SR_TRANS_AUTO_CHRYSLER: return "Auto Trnsmsn Chrysler.";
                case SR_TRANS_3SPD_GM: return "Trnsmsn 3-spd GM.";
                case SR_TRANS_3SPD_FORD: return "Trnsmsn 3-spd Ford.";
                case SR_TRANS_3SPD_CHRYSLER: return "Trnsmsn 3-spd Chrysler.";
                case SR_TRANS_4SPD_GM: return "Trnsmsn 4-spd GM.";
                case SR_TRANS_4SPD_FORD: return "Trnsmsn 4-spd Ford.";
                case SR_TRANS_4SPD_CHRYSLER: return "Trnsmsn 4-spd Chrysler.";
                case SR_TRANS_4SPD_UNIVERSAL: return "Trnsmsn 4-spd Fits all.";
                case SR_TRANS_RACING_GM: return "Racing 4-spd GM.";
                case SR_TRANS_RACING_FORD: return "Racing 4-spd Ford.";
                case SR_TRANS_RACING_CHRYSLER: return "Racing 4-spd Chrysler.";
                default: {
                    // Fallback to vendor auto
                    std::string v = vendorToStr(vendor);
                    return std::string("Auto Trnsmsn ") + v + ".";
                }
            }
        };
        auto carbName = [](AmigaLibAsset aid) -> const char* {
            switch (aid) {
                case ENGINE_CARB_2BARREL: return "2-brl carb. Fits all.";
                case ENGINE_CARB_4BARREL: return "4-brl carb.";
                case ENGINE_CARB_4BARREL_RACING: return "Racing 4-brl carb.";
                default: return "carburetor.";
            }
        };

        CarVendor_t vendor = model.vendor;
        if (vendor == CarVendorUnknown) {
            vendor = SrLibQueryGetCarVendor(model.index);
        }

        // Debug: Car Vendor
        std::string vendorLabel = "Car Vendor: " + std::string(vendorToStr(vendor));
        addLeft(vendorLabel.c_str());
        
        // Engine line
        EnginePart engine = car->GetEngine();
        std::string engineLabel = engine.GetEngineName();
        addRightNote(engineLabel.c_str(), wearNote(engine.wear).c_str());
        // Manifold line (based on carb type if possible)
        const Part carb = car->GetCarburator();
        const char* maniText = (carb.assetId == ENGINE_CARB_2BARREL) ? "  2-brl manifold." : "  4-brl manifold.";
        addIndented(maniText);
        // Carb line
        std::string carbText = std::string("  ") + carbName(carb.assetId);
        addIndented(carbText.c_str());
        // Transmission line
        const Part trans = car->GetGearbox();
        std::string transText = transName(trans.assetId, vendor);
        addRightNote(transText.c_str(), wearNote(trans.wear).c_str());
        // Tires line
        addRightNote("Tires.", wearNote(car->GetTires().wear).c_str());

        // Blank line
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        // Fuel (placeholder until wired)
        addLeft("Fuel     4 gallons");
        // Blank line
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        int mph = model.baseSpeed > 0 ? model.baseSpeed : 97;
        char speedBuf[64];
        snprintf(speedBuf, sizeof(speedBuf), "Max speed   %d mph", mph);
        addLeft(speedBuf);
    }
    
    DialogConfig infoCfg;
    infoCfg.bounds = dialogRect;
    infoCfg.backgroundColor = {0, 208, 0, 255};
    infoCfg.borderColor = {0, 111, 239, 255};
    infoCfg.contentAreas = { dialogRect }; // draw dialog background
    infoCfg.texts = texts;
    infoCfg.lines = lines;
    infoCfg.font = state.GetAssets().GetMainFont();
    
    // No buttons - dialog is dismissed by clicking outside or pressing Escape
    infoCfg.buttons = {};
    infoCfg.onZoneHover = nullptr;
    infoCfg.onButtonPressed = nullptr;
    infoCfg.onDismiss = [this]() {
        carInfoDialog->Hide();
        SetActiveDialog(nullptr);
    };

    carInfoDialog = std::make_unique<Dialog>(infoCfg);
    carInfoDialog->Show();
    SetActiveDialog(carInfoDialog.get());
}

void Garage::LoadCustomStickers() {
    customStickerBitmaps.clear();
    customStickerPaths.clear();

    namespace fs = std::filesystem;
    const fs::path stickerDir = state.GetAssets().GetBasePath() / "stickers";
    LOG_INFO("LoadCustomStickers: looking in %s", stickerDir.string().c_str());
    if (!fs::exists(stickerDir) || !fs::is_directory(stickerDir)) {
        LOG_INFO("LoadCustomStickers: directory not found");
        return;
    }

    std::vector<fs::path> pngFiles;
    for (const auto& entry : fs::directory_iterator(stickerDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            // Case-insensitive .png check
            if (ext == ".png" || ext == ".PNG") {
                pngFiles.push_back(entry.path());
            }
        }
    }
    std::sort(pngFiles.begin(), pngFiles.end());

    for (const auto& p : pngFiles) {
        if (customStickerBitmaps.size() >= 16) break;
        auto bmp = std::make_unique<Bitmap>();
        bmp->LoadBitmap(p.string());
        if (bmp->GetSurface()) {
            int w = bmp->GetSurface()->w;
            int h = bmp->GetSurface()->h;
            if (w > 72 || h > 25) {
                LOG_INFO("Skipping custom sticker %s: %dx%d exceeds 72x25 limit", p.string().c_str(), w, h);
                continue;
            }
            customStickerPaths.push_back(p.string());
            customStickerBitmaps.push_back(std::move(bmp));
        } else {
            LOG_INFO("Failed to load custom sticker: %s", p.string().c_str());
        }
    }
    LOG_INFO("Loaded %zu custom stickers from data/stickers/", customStickerBitmaps.size());
}

void Garage::ShowStickerDialog() {
    // Only reload custom stickers on first open (scrollRow == 0 means fresh open)
    if (stickerScrollRow == 0) {
        LoadCustomStickers();
    }

    Spritesheet* decalsSheet = state.GetAssets().GetCarDecals();
    if (!decalsSheet) {
        LOG_INFO("ShowStickerDialog: DECALS bitmap not loaded");
        return;
    }

    const SDL_Color frameBg = {0x2A, 0x2A, 0x2A, 0xFF};
    const SDL_Color frameBorder = {0x88, 0x88, 0x88, 0xFF};

    const int SRC_CELL_W = 55;
    const int SRC_CELL_H = 25;
    const int VISIBLE_ROWS = 2;

    int dosDecalCount = state.GetAssets().HasDosDecals() ? 8 : 0;
    int customCount = (int)customStickerBitmaps.size();
    int totalItems = 8 + dosDecalCount + customCount;  // 8 Amiga stock + DOS + custom
    int totalRows = (totalItems + 3) / 4;

    // Clamp scroll position
    int maxScrollRow = std::max(0, totalRows - VISIBLE_ROWS);
    if (stickerScrollRow > maxScrollRow) stickerScrollRow = maxScrollRow;
    if (stickerScrollRow < 0) stickerScrollRow = 0;

    const int cellW = 264 / 4;
    const int cellH = SRC_CELL_H + 8;
    int dialogH = VISIBLE_ROWS * cellH + 4;

    SDL_Rect dialogRect = {23, 11, 264, dialogH};
    const int GRID_LEFT = dialogRect.x + 2;
    const int GRID_TOP = dialogRect.y + 2;

    std::vector<SDL_Rect> contentAreas = {dialogRect};
    std::map<std::string, DialogBitmap> bitmaps;
    std::vector<Zone> zones;

    const ZoneId decalZoneIds[] = {
        ZONE_GARAGE_STICKER_DECAL_0, ZONE_GARAGE_STICKER_DECAL_1, ZONE_GARAGE_STICKER_DECAL_2, ZONE_GARAGE_STICKER_DECAL_3,
        ZONE_GARAGE_STICKER_DECAL_4, ZONE_GARAGE_STICKER_DECAL_5, ZONE_GARAGE_STICKER_DECAL_6, ZONE_GARAGE_STICKER_DECAL_7
    };
    const ZoneId dosZoneIds[] = {
        ZONE_GARAGE_STICKER_DOS_0, ZONE_GARAGE_STICKER_DOS_1, ZONE_GARAGE_STICKER_DOS_2, ZONE_GARAGE_STICKER_DOS_3,
        ZONE_GARAGE_STICKER_DOS_4, ZONE_GARAGE_STICKER_DOS_5, ZONE_GARAGE_STICKER_DOS_6, ZONE_GARAGE_STICKER_DOS_7
    };
    const ZoneId customZoneIds[] = {
        ZONE_GARAGE_STICKER_CUSTOM_0, ZONE_GARAGE_STICKER_CUSTOM_1, ZONE_GARAGE_STICKER_CUSTOM_2, ZONE_GARAGE_STICKER_CUSTOM_3,
        ZONE_GARAGE_STICKER_CUSTOM_4, ZONE_GARAGE_STICKER_CUSTOM_5, ZONE_GARAGE_STICKER_CUSTOM_6, ZONE_GARAGE_STICKER_CUSTOM_7,
        ZONE_GARAGE_STICKER_CUSTOM_8, ZONE_GARAGE_STICKER_CUSTOM_9, ZONE_GARAGE_STICKER_CUSTOM_10, ZONE_GARAGE_STICKER_CUSTOM_11,
        ZONE_GARAGE_STICKER_CUSTOM_12, ZONE_GARAGE_STICKER_CUSTOM_13, ZONE_GARAGE_STICKER_CUSTOM_14, ZONE_GARAGE_STICKER_CUSTOM_15,
    };

    // Populate visible cells based on scroll position
    // Layout: items 0-7 = Amiga stock, 8-15 = DOS (if available), then custom
    for (int displayRow = 0; displayRow < VISIBLE_ROWS; displayRow++) {
        int actualRow = stickerScrollRow + displayRow;
        for (int c = 0; c < 4; c++) {
            int itemIndex = actualRow * 4 + c;
            if (itemIndex >= totalItems) break;

            SDL_Rect dest = {GRID_LEFT + c * cellW, GRID_TOP + displayRow * cellH, cellW, cellH};

            if (itemIndex < 8) {
                // Amiga stock sticker from spritesheet
                int stockRow = itemIndex / 4;
                int stockCol = itemIndex % 4;
                SDL_Rect src = {stockCol * SRC_CELL_W, stockRow * SRC_CELL_H, SRC_CELL_W, SRC_CELL_H};
                DialogBitmap db;
                db.bitmap = decalsSheet;
                db.rect = dest;
                db.zOrder = 0;
                db.sourceRect = src;
                bitmaps["decal_" + std::to_string(itemIndex)] = db;
                zones.push_back(Zone(decalZoneIds[itemIndex], strdup(SrLibCarDecalNames[itemIndex]), &dest));
            } else if (dosDecalCount > 0 && itemIndex < 8 + dosDecalCount) {
                // DOS sticker - use native dimensions, center in cell, scale only if too big
                int dosIndex = itemIndex - 8;
                Bitmap* dosBmp = state.GetAssets().GetDosDecal(dosIndex);
                if (dosBmp) {
                    SDL_Rect native = dosBmp->GetSourceRect();
                    int drawW = native.w;
                    int drawH = native.h;
                    // Scale down proportionally if larger than cell
                    if (drawW > cellW || drawH > cellH) {
                        float scaleW = (float)cellW / drawW;
                        float scaleH = (float)cellH / drawH;
                        float scale = std::min(scaleW, scaleH);
                        drawW = (int)(drawW * scale);
                        drawH = (int)(drawH * scale);
                    }
                    // Center in cell
                    SDL_Rect centered = {
                        dest.x + (cellW - drawW) / 2,
                        dest.y + (cellH - drawH) / 2,
                        drawW, drawH
                    };
                    DialogBitmap db;
                    db.bitmap = dosBmp;
                    db.rect = centered;
                    db.zOrder = 0;
                    bitmaps["dos_" + std::to_string(dosIndex)] = db;
                    zones.push_back(Zone(dosZoneIds[dosIndex], strdup(SrLibCarDecalNames[8 + dosIndex]), &dest));
                }
            } else {
                // Custom sticker
                int customIndex = itemIndex - 8 - dosDecalCount;
                DialogBitmap db;
                db.bitmap = customStickerBitmaps[customIndex].get();
                db.rect = dest;
                db.zOrder = 0;
                bitmaps["custom_" + std::to_string(customIndex)] = db;
                std::string stickerName = std::filesystem::path(customStickerPaths[customIndex]).stem().string();
                if (!stickerName.empty()) {
                    std::transform(stickerName.begin(), stickerName.end(), stickerName.begin(), ::tolower);
                    stickerName[0] = std::toupper(stickerName[0]);
                }
                zones.push_back(Zone(customZoneIds[customIndex], strdup(stickerName.c_str()), &dest));
            }
        }
    }

    // Scroll buttons outside dialog, same pattern as car picker
    const int scrollX = 289;
    const int scrollBtnW = 15;
    const int scrollBtnH = 15;
    SDL_Rect scrollTopRect    = {scrollX, dialogRect.y, scrollBtnW, scrollBtnH};
    SDL_Rect scrollUpRect     = {scrollX, dialogRect.y + 16, scrollBtnW, scrollBtnH};
    SDL_Rect scrollDownRect   = {scrollX, dialogRect.y + dialogH - 31, scrollBtnW, scrollBtnH};
    SDL_Rect scrollBottomRect = {scrollX, dialogRect.y + dialogH - 15, scrollBtnW, scrollBtnH};

    zones.push_back(Zone(ZONE_GARAGE_STICKER_SCROLL_TOP, strdup("\\"), &scrollTopRect));
    zones.push_back(Zone(ZONE_GARAGE_STICKER_SCROLL_UP, strdup("<"), &scrollUpRect));
    zones.push_back(Zone(ZONE_GARAGE_STICKER_SCROLL_DOWN, strdup(">"), &scrollDownRect));
    zones.push_back(Zone(ZONE_GARAGE_STICKER_SCROLL_BOTTOM, strdup("/"), &scrollBottomRect));

    std::vector<ButtonConfig> buttons;
    ButtonConfig scrollTopBtn = {scrollTopRect, "\\", nullptr, false, 1};
    scrollTopBtn.fillColor = frameBg;
    scrollTopBtn.borderColor = frameBorder;
    scrollTopBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollUpBtn = {scrollUpRect, "<", nullptr, false, 2};
    scrollUpBtn.fillColor = frameBg;
    scrollUpBtn.borderColor = frameBorder;
    scrollUpBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollDownBtn = {scrollDownRect, ">", nullptr, false, 3};
    scrollDownBtn.fillColor = frameBg;
    scrollDownBtn.borderColor = frameBorder;
    scrollDownBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollBottomBtn = {scrollBottomRect, "/", nullptr, false, 4};
    scrollBottomBtn.fillColor = frameBg;
    scrollBottomBtn.borderColor = frameBorder;
    scrollBottomBtn.textColor = ButtonDefaultChoiceColor;
    buttons = {scrollTopBtn, scrollUpBtn, scrollDownBtn, scrollBottomBtn};

    DialogConfig stickerCfg;
    // Bounds includes scroll buttons for dismiss hit-testing; contentAreas is the visual rect
    stickerCfg.bounds = {dialogRect.x, dialogRect.y, (scrollX + scrollBtnW) - dialogRect.x, dialogH};
    stickerCfg.backgroundColor = frameBg;
    stickerCfg.borderColor = frameBorder;
    stickerCfg.contentAreas = contentAreas;
    stickerCfg.bitmaps = bitmaps;
    stickerCfg.zones = zones;
    stickerCfg.buttons = buttons;
    stickerCfg.font = state.GetAssets().GetMainFont();
    stickerCfg.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText());
    };
    stickerCfg.onButtonPressed = [this, maxScrollRow](int index) {
        switch (index) {
            case 1: // TOP
                if (stickerScrollRow > 0) {
                    stickerScrollRow = 0;
                    stickerNeedsRefresh = true;
                }
                break;
            case 2: // UP
                if (stickerScrollRow > 0) {
                    stickerScrollRow--;
                    stickerNeedsRefresh = true;
                }
                break;
            case 3: // DOWN
                if (stickerScrollRow < maxScrollRow) {
                    stickerScrollRow++;
                    stickerNeedsRefresh = true;
                }
                break;
            case 4: // BOTTOM
                if (stickerScrollRow < maxScrollRow) {
                    stickerScrollRow = maxScrollRow;
                    stickerNeedsRefresh = true;
                }
                break;
        }
    };
    stickerCfg.onZoneClick = [this](Zone& zone) {
        ZoneId id = zone.GetId();

        // Handle stock decals
        if (id >= ZONE_GARAGE_STICKER_DECAL_0 && id <= ZONE_GARAGE_STICKER_DECAL_7) {
            int decalIndex = id - ZONE_GARAGE_STICKER_DECAL_0;
            Car* car = state.GetCurrentCar();
            if (car && car->CanAddDecal()) {
                int money = state.GetBankroll();
                if (money >= 5) {
                    state.SetBankroll(money - 5);
                    car->SetDecal((SrLibCarDecal)decalIndex);
                    car->SetCustomDecalPath("");
                    GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                    GetOrCreateCarBitmap(car)->StartStickerWipe();
                }
            }
            stickerScrollRow = 0;
            stickerDialog->Hide();
            SetActiveDialog(nullptr);
            return;
        }

        // Handle DOS decals
        if (id >= ZONE_GARAGE_STICKER_DOS_0 && id <= ZONE_GARAGE_STICKER_DOS_7) {
            int dosIndex = id - ZONE_GARAGE_STICKER_DOS_0;
            Car* car = state.GetCurrentCar();
            if (car && car->CanAddDecal()) {
                int money = state.GetBankroll();
                if (money >= 5) {
                    state.SetBankroll(money - 5);
                    car->SetDecal((SrLibCarDecal)(dosIndex + SrLibCarDecalDosSeDecal1));
                    car->SetCustomDecalPath("");
                    GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                    GetOrCreateCarBitmap(car)->StartStickerWipe();
                }
            }
            stickerScrollRow = 0;
            stickerDialog->Hide();
            SetActiveDialog(nullptr);
            return;
        }

        // Handle custom stickers
        if (id >= ZONE_GARAGE_STICKER_CUSTOM_0 && id <= ZONE_GARAGE_STICKER_CUSTOM_15) {
            int customIndex = id - ZONE_GARAGE_STICKER_CUSTOM_0;
            if (customIndex >= 0 && customIndex < (int)customStickerPaths.size()) {
                Car* car = state.GetCurrentCar();
                if (car && car->CanAddDecal()) {
                    int money = state.GetBankroll();
                    if (money >= 5) {
                        state.SetBankroll(money - 5);
                        car->SetDecal(SrLibCarDecalCustom);
                        car->SetCustomDecalPath(customStickerPaths[customIndex]);
                        GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                        GetOrCreateCarBitmap(car)->StartStickerWipe();
                    }
                }
            }
            stickerScrollRow = 0;
            stickerDialog->Hide();
            SetActiveDialog(nullptr);
            return;
        }
    };
    stickerCfg.onDismiss = [this]() {
        stickerScrollRow = 0;
        stickerDialog->Hide();
        SetActiveDialog(nullptr);
    };

    stickerDialog = std::make_unique<Dialog>(stickerCfg);
    stickerDialog->Show();
    SetActiveDialog(stickerDialog.get());
}

void Garage::ShowModificationDialog(const char* title, int price, std::function<void()> onConfirm, std::unique_ptr<Dialog>& dialogPtr) {
    // Dialog dimensions as specified
    const int dialogWidth = 156;
    const int dialogHeight = 36;
    const int dialogX = 82;
    const int dialogY = 50;
    
    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};
    
    // Check if player has enough money
    // int currentMoney = state.GetBankroll();
    // bool canAfford = currentMoney >= price;
    
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
    modDialogConfig.font = state.GetAssets().GetMainFont();
    modDialogConfig.onButtonPressed = [this, price, onConfirm, &dialogPtr](int index) {
        switch (index) {
            case 0: // OK
                {
                    int currentMoney = state.GetBankroll();
                    if (currentMoney < price) {
                        // Show "Customization ain't free!" bubble and keep dialog open
                        LOG_INFO("Not enough money! Need $%d", price);
                        if (bubbles.size() > 4) {
                            bubbles[4]->SetVisible(true);  // customizationBubble
                        }
                        return;
                    }
                    
                    // Deduct price
                    state.SetBankroll(currentMoney - price);
                    
                    // Execute the modification
                    onConfirm();
                    
                    // Hide bubble and close dialog
                    if (bubbles.size() > 4) {
                        bubbles[4]->SetVisible(false);
                    }
                    dialogPtr->Hide();
                    SetActiveDialog(nullptr);
                }
                break;
            case 1: // Forget it
                // Hide bubble when closing
                if (bubbles.size() > 4) {
                    bubbles[4]->SetVisible(false);
                }
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

    LOG_INFO("Garage::UpdateSign");
    char text[50];
    CarModel m = state.GetCurrentCar()->GetModel();
    snprintf(text, 50, "%s.", m.name.c_str());
    
    // Clear the surface
    Uint32 transparent = SDL_MapRGBA(signSurface->format, 0, 0, 0, 0);
    SDL_FillRect(signSurface, NULL, transparent);
    
    // Write new text
    int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), text);
    int x = signRect.x + (signRect.w - textWidth) / 2;
    SFont_Write(signSurface, state.GetAssets().GetMainFont(), x, 45, text);
    
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
    case ZONE_GARAGE_CAR_INFO:
        ShowCarInfoDialog();
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
    case ZONE_GARAGE_PAINT_JOB:
        ShowPaintDialog();
        break;
    case ZONE_GARAGE_STICKERS:
        stickerScrollRow = 0;
        ShowStickerDialog();
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
        if (carBitmap->IsStickerWipeActive()) {
            carBitmap->UpdateStickerWipe(deltaTime);
        }
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

    // Draw paint can preview if paint dialog is active
    if (paintCanPreview && paintCanPreview->IsVisible()) {
        paintCanPreview->Draw();
    }

    // Deferred sticker dialog refresh (avoids use-after-free when scrolling)
    if (stickerNeedsRefresh) {
        stickerNeedsRefresh = false;
        stickerDialog->Hide();
        ShowStickerDialog();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    // Draw bubbles last so they appear on top of dialogs
    for (auto& b : bubbles) {
        b->Draw();
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

bool Garage::PushEvent(SDL_Event* event) {
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return true;
    }
    return CarScene::PushEvent(event);
}

void Garage::OnCarAnimationComplete() {
    LOG_INFO("Garage::OnCarAnimationComplete - Entry point");
    CarScene::OnCarAnimationComplete();
    if (pendingCar != nullptr) {
        state.SetCurrentCar(pendingCar);
        
        UpdateSign();
        
        // Mark that we need a new animation (important for parent's StartAnimateCar logic)
        needsNewAnimation = true;
        
        // Start the drive-in animation for the new car (this will set up audio, bitmap, position, etc.)
        LOG_INFO("Garage::OnCarAnimationComplete - Starting drive-in animation for new car");
        StartAnimateCar(pendingCar, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
        
        pendingCar = nullptr;
    }
    
    // Update paint preview color to match the current car
    if (carBitmap) {
        StandardCarColor newColor = carBitmap->GetCurrentColor();
        LOG_INFO("OnCarAnimationComplete: carBitmap color is %d, updating paint can", (int)newColor);
        previewColor = newColor;
        originalColor = previewColor;
        UpdatePaintCanPreview();
    }
    
    // Update car zones after animation completes
    UpdateCarZones();
}

void Garage::UpdateCarZones() {
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
    
    LOG_INFO("Garage::StartAnimateCar - car=%p, type=%d", (void*)car, animType);
    
    // Garage-specific setup BEFORE calling parent
    // Create/get car bitmap
    carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), state.GetCurrentCar()->GetModel().index);
    
    // Set initial position based on animation type
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
        // Reverse: start at idle position (car is already visible)
        SDL_Point idlePos = GetCarIdlePosition();
        carx = idlePos.x;
        LOG_INFO("Garage::StartAnimateCar - Reverse animation, car at idle x=%d", carx);
    } else {
        // Drive in: start off-screen left
        carx = -GAME_WIDTH;
        LOG_INFO("Garage::StartAnimateCar - Drive in animation, car off-screen x=%d", carx);
    }
    
    carBitmap->Move({carx, cary});
    
    // Call parent method to start engine sound and set up animation
    // Parent will NOT override our position for reverse animations
    CarScene::StartAnimateCar(car, driver, animType);
    
    // Update sign for initial car
    UpdateSign();
    
    currentAnimationType = animType;
    LOG_INFO("Garage::StartAnimateCar - Animation started");
}