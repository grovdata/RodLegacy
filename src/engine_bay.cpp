#include "state.h"
#include "engine_bay.h"
#include "animated_bitmap.h"

const int buttonWidth = 39;
const int buttonHeight = 9;

EngineBay::EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback)
    : state(&state), currentState(ENGINE_BAY_OVERVIEW) {

    // Initial level
    this->tuneLevel = tuneLevel;
    this->engineSpeed = tuneLevel;
    
    // Common dialog configuration
    SDL_Rect engineBayRect = {24, 0, 200, 189};
    SDL_Rect engineBayFillRect = {28, 4, 193, 181};
    SDL_Rect sidebarRect = {225, 1, 71, 189};
    SDL_Rect gapRect = {223, 0, 2, 187};

    SDL_Rect tuneRect = {243, 31, buttonWidth, buttonHeight};
    SDL_Rect partsRect = {243, 90, buttonWidth, buttonHeight};
    SDL_Rect doneRect = {243, 150, buttonWidth, buttonHeight};
    SDL_Rect doneRectTune = partsRect;

    SDL_Rect engineBackdropRect = {34,31,168,152};
    SDL_Rect retardRect = {98, 9, 10, 11};
    SDL_Rect advanceRect = {143, 9, 10, 11};
    SDL_Rect tuningGadgetRect = {98, 9, 56, 11};
    SDL_Rect beltAnimationRect = {65, 37, 85, 53};
    SDL_Rect leftPistonRect = {52, 68, 42, 55};
    SDL_Rect rightPistonRect = {135, 68, 41, 54};
    SDL_Rect crankRect = {80, 122, 71, 33};
    SDL_Rect flywheelRect = {150,122,43,34};
    SDL_Rect camRect = {96,110,38,14};
    SDL_Rect tuneKnobRect = {124, 22, 16, 10};


    Zone tuneZone("tune", "Tune", tuneRect);
    Zone partsZone("parts", "Parts", partsRect);
    Zone doneZone("done", "Done", doneRect);
    Zone retardZone("retard", "Retard", retardRect);
    Zone advanceZone("advance", "Advance", advanceRect);
    Zone doneTuneZone("done_tune", "Done", partsRect);

    std::vector<Zone> engineDialogZones = {tuneZone, partsZone, doneZone};
    std::vector<Zone> tuneDialogZones = {retardZone, advanceZone, doneTuneZone};

    // Common button configuration
    Bitmap* buttonBitmap = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS});
    Bitmap* buttonBitmapDone = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS_DONE});
    
    std::vector<ButtonConfig> overviewButtons = {
        {
            .bounds = tuneRect,
            .text = "Tune",
            .onClick = nullptr,
            .defaultChoice = true,
            .index = 0,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmap, {18, 31, buttonWidth, buttonHeight}}
        },
        {
            .bounds = partsRect,
            .text = "Parts",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 1,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmap, {18, 90, buttonWidth, buttonHeight}}
        },
        {
            .bounds = doneRect,
            .text = "Done",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 2,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmap, {18, 150, buttonWidth, buttonHeight}}
        }
    };

    std::vector<ButtonConfig> tuningButtons;
    tuningButtons = {
        {
            .bounds = retardRect,
            .text = "Retard",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 0,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmapDone, {98, 9, 10, 11}}
        },
        {
            .bounds = advanceRect,
            .text = "Advance",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 1,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmapDone, {143, 9, 10, 11}}
        },
        {
            .bounds = doneRectTune,
            .text = "Done",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 2,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmapDone, {18, 150, buttonWidth, buttonHeight}}
        }
    };

    // Debug: Log the tuning button indices
    LOG_INFO("Tuning buttons created:");
    for (size_t i = 0; i < tuningButtons.size(); i++) {
        LOG_INFO("  Button %zu: %s (index: %d)", i, tuningButtons[i].text.c_str(), tuningButtons[i].index);
    }

    // Common callback setup
    auto buttonCallbackWrapper = [buttonCallback](const int buttonIndex) {
        LOG_INFO("=== OVERVIEW BUTTON CALLBACK ===");
        LOG_INFO("Overview button index: %d", buttonIndex);
        if (buttonIndex == 0) { // Tune button - handled by caller
            LOG_INFO("Tune button pressed in overview");
        }
        if (buttonCallback) {
            LOG_INFO("Calling original buttonCallback from overview");
            buttonCallback(buttonIndex);
        }
        LOG_INFO("=== END OVERVIEW BUTTON CALLBACK ===");
    };
    
    // Tuning-specific callback setup
    auto tuningButtonCallbackWrapper = [this, buttonCallback](const int buttonIndex) {
        LOG_INFO("=== TUNING BUTTON CALLBACK ===");
        LOG_INFO("tunelevel %d, buttonIndex: %d", this->tuneLevel, buttonIndex);
        if (buttonIndex == 0) { // Retard button
            LOG_INFO("Retard button pressed - decreasing tune level");
            SetKnobPosition(this->tuneLevel - 1);
            // Don't call original callback for Retard button
        } else if (buttonIndex == 1) { // Advance button
            LOG_INFO("Advance button pressed - increasing tune level");
            SetKnobPosition(this->tuneLevel + 1);
            // Don't call original callback for Advance button
        } else if (buttonIndex == 2) { // Done button
            LOG_INFO("Done button pressed - switching back to overview");
            // Switch back to overview dialog
            SwitchToState(ENGINE_BAY_OVERVIEW);
            LOG_INFO("Current state after switch: %d", this->currentState);
            // Call original callback with a special index to just update the dialog
            if (buttonCallback) {
                buttonCallback(99); // Special index to just update dialog without state change
            }
        } else {
            LOG_INFO("Unknown button index: %d", buttonIndex);
            if (buttonCallback) {
                buttonCallback(buttonIndex);
            }
        }
        LOG_INFO("=== END TUNING BUTTON CALLBACK ===");
    };
    
    auto zoneHoverWrapper = zoneHoverCallback ? zoneHoverCallback : [](Zone& zone) {
        LOG_INFO("Engine Bay Zone Hover: %s (Zone ID: %s)", zone.GetText().c_str(), zone.GetId().c_str());
    };

    // Create overview dialog
    DialogConfig overviewConfig;
    overviewConfig.bounds = engineBayRect;
    overviewConfig.backgroundColor = {0, 0, 0, 255};
    overviewConfig.borderColor = {0, 0, 0, 0};
    overviewConfig.buttons = overviewButtons;
    overviewConfig.zones = engineDialogZones;
    overviewConfig.contentAreas = std::vector<SDL_Rect>{engineBayFillRect, sidebarRect, gapRect};
    overviewConfig.font = state.GetAssets().GetMainFont();
    overviewConfig.onButtonPressed = buttonCallbackWrapper;
    overviewConfig.onZoneHover = zoneHoverWrapper;

    // Overview dialog bitmaps
    overviewConfig.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    overviewConfig.bitmaps["button_sidebar"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS}),
        {225, 0, 71, 190}, 1
    };
    overviewConfig.bitmaps["engine_test_v6"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_V6}),
        {62, 51, 136, 112}, 2
    };
    overviewConfig.bitmaps["manifold"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_MANIFOLD5}),
        {93, 52, 88, 31}, 2
    };
    overviewConfig.bitmaps["carb2brl"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CARB_2BARREL}),
        {116, 35, 32, 29}, 4
    };
    overviewConfig.bitmaps["engine_wire_attached"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED}),
        {162, 149, 40, 21}, 3
    };
    overviewConfig.bitmaps["bolt1"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT}),
        {162, 180, 8, 4}, 3,
        {0, 0, 8, 4}  // crop to just the bolt head (rows 0-3 of the 8x12 asset)
    };

    engineDialog = new Dialog(overviewConfig);

    // Create tuning dialog
    DialogConfig tuningConfig = overviewConfig; // Copy common config
    
    // Update buttons for tuning dialog
    tuningConfig.buttons = tuningButtons;
    tuningConfig.zones = tuneDialogZones;
    tuningConfig.onButtonPressed = tuningButtonCallbackWrapper;
    
    // Replace sidebar and content bitmaps for tuning
    tuningConfig.bitmaps.clear();
    tuningConfig.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    tuningConfig.bitmaps["button_sidebar_done"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS_DONE}),
        {225, 0, 71, 190}, 1
    };
    tuningConfig.bitmaps["tuning_gadget"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNE_GADGET}),
        tuningGadgetRect
    };

    tuningConfig.bitmaps["engine_backdrop"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNING_SCREEN}),
        engineBackdropRect
    };

    // Create animated engine tune bitmap - using animation data from libp.h
    Bitmap* engineTuneAnimationBelt = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BELT_ANIMATION});
    AnimatedBitmap* engineTuneAnimation = new AnimatedBitmap(engineTuneAnimationBelt, beltAnimationRect);
    engineTuneAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_BELT_ANIMATION));
    engineTuneAnimation->Play();

    tuningConfig.bitmaps["engine_tune_animation"] = {
        engineTuneAnimation,
        beltAnimationRect
    };

    Bitmap* leftPiston = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_LEFT_ANIMATION});
    AnimatedBitmap *leftPistonAnimation = new AnimatedBitmap(leftPiston, leftPistonRect);
    leftPistonAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_PISTON_LEFT_ANIMATION));
    leftPistonAnimation->Play();

    tuningConfig.bitmaps["left_piston_animation"] = {
        leftPistonAnimation,
        leftPistonRect,
        5
    };

    Bitmap* rightPiston = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_RIGHT_ANIMATION});
    AnimatedBitmap *rightPistonAnimation = new AnimatedBitmap(rightPiston, rightPistonRect);
    rightPistonAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_PISTON_RIGHT_ANIMATION));
    rightPistonAnimation->Play();

    tuningConfig.bitmaps["right_piston_animation"] = {
        rightPistonAnimation,
        rightPistonRect,
        5
    };

    Bitmap* crank = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CRANK_ANIMATION});
    AnimatedBitmap *crankAnimation = new AnimatedBitmap(crank, crankRect);
    crankAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_CRANK_ANIMATION));
    crankAnimation->Play();

    tuningConfig.bitmaps["crank_animation"] = {
        crankAnimation,
        crankRect,
        5
    };

    Bitmap* flywheel = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_FLYWHEEL_ANIMATION});
    AnimatedBitmap *flywheelAnimation = new AnimatedBitmap(flywheel, flywheelRect);
    flywheelAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_FLYWHEEL_ANIMATION));
    flywheelAnimation->Play();

    tuningConfig.bitmaps["flywheel_animation"] = {
        flywheelAnimation,
        flywheelRect,
        5
    };

    Bitmap* camshaft = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CAM_ANIMATION});
    AnimatedBitmap *camshaftAnimation = new AnimatedBitmap(camshaft, camRect);
    camshaftAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_CAM_ANIMATION));
    camshaftAnimation->Play();

    tuningConfig.bitmaps["cam_animation"] = {
        camshaftAnimation,
        camRect,
        5
    };

    Bitmap* tuneKnob = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_TUNE_INDICATOR});
    tuningConfig.bitmaps["tune_knob"] = {
        tuneKnob,
        tuneKnobRect,
        5
    };


    tuneDialog = new Dialog(tuningConfig);
    
    // Create parts dialog
    InitPartsDialog();
    
    // Set initial knob position
    SetKnobPosition(tuneLevel);
}

EngineBay::~EngineBay() {
    // Don't delete dialogs - they're owned by the Garage class
}

void EngineBay::SwitchToState(EngineBayState newState) {
    if (currentState == newState) {
        return; // Already in this state
    }
    
    LOG_INFO("EngineBay::SwitchToState: %d -> %d", currentState, newState);
    currentState = newState;
}

void EngineBay::ResetToOverview() {
    currentState = ENGINE_BAY_OVERVIEW;
    LOG_INFO("EngineBay::ResetToOverview: Reset to overview state");
}

Dialog* EngineBay::GetDialog() {
    switch (currentState) {
        case ENGINE_BAY_TUNING:
            return tuneDialog;
        case ENGINE_BAY_PARTS:
            return partsDialog.get();
        case ENGINE_BAY_OVERVIEW:
        default:
            return engineDialog;
    }
}

void EngineBay::OnDialogShown() {
    // No need to restore visibility since we're using separate dialogs
}

void EngineBay::SetKnobPosition(int level) {
    if (level < 0) level = 0;
    if (level > 12) level = 12;
    
    tuneLevel = level;
    engineSpeed = level * 0.0625f;

    if (state->GetCurrentCar()) {
        state->GetCurrentCar()->SetTuneLevel(level);
    }

    // Update animation speeds for all engine animations
    // Higher tune level = faster animations, so use inverse of engineSpeed for frame duration
    const char* animationNames[] = {
        "engine_tune_animation", "left_piston_animation", "right_piston_animation",
        "crank_animation", "flywheel_animation", "cam_animation"
    };
    
    for (const char* name : animationNames) {
        DialogBitmap* bitmap = tuneDialog->GetBitmap(name);
        if (bitmap && bitmap->bitmap) {
            if (AnimatedBitmap* anim = dynamic_cast<AnimatedBitmap*>(bitmap->bitmap)) {
                // Invert the speed: higher engineSpeed = lower frameDuration = faster animation
                float frameDuration = 1.0f - engineSpeed;
                anim->SetSpeed(frameDuration);
            }
        }
    }

    DialogBitmap* knobBitmap = tuneDialog->GetBitmap("tune_knob");
    if (knobBitmap) {
        int baseX = 108;  // Starting X position
        int baseY = 22;   // Y position (unchanged)
        int newX = baseX + (level * 2);
        knobBitmap->bitmap->SetPosition(newX, baseY);
    }
}

void EngineBay::InitPartsDialog() {
    // Parts dialog configuration
    SDL_Rect partsDialogRect = {36, 26, 260, 136};
    SDL_Rect partsListRect = {40, 40, 242, 80};
    SDL_Rect topRect = {37, 27, 246, 95};
    SDL_Rect bottomRect = {37, 123, 246, 40};
    SDL_Rect selectedPartRect = {39, 127, 242, 12};
    
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int buttonY = partsDialogRect.y + partsDialogRect.h - buttonHeight - 8;

    SDL_Rect changeRect = {partsDialogRect.x + 34, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {partsDialogRect.x + 135, buttonY, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect = {partsDialogRect.x + partsDialogRect.w + 2, partsDialogRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect = {partsDialogRect.x + partsDialogRect.w + 2, partsDialogRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect = {partsDialogRect.x + partsDialogRect.w + 2, partsDialogRect.y + partsDialogRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {partsDialogRect.x + partsDialogRect.w + 2, partsDialogRect.y + partsDialogRect.h - 15, 15, 15};
    
    // Sample parts list (you can replace this with actual parts from the car/state)
    std::vector<std::string> partsLabels = {
        "Engine Block",
        "Carburetor",
        "Exhaust",
        "Transmission",
        "Tires",
        "Brakes",
        "Suspension",
        "Radiator",
        "Battery",
        "Alternator",
        "Starter",
        "Fuel Pump"
    };



    
    std::vector<ButtonConfig> partsButtons = {
        {
            .bounds = changeRect,
            .text = "Change",
            .onClick = nullptr,
            .defaultChoice = true,
            .index = 0,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = false,
            .bitmapConfig = {nullptr, {0, 0, 0, 0}}
        },
        {
            .bounds = forgetItRect,
            .text = "Forget it",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 1,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = false,
            .bitmapConfig = {nullptr, {0, 0, 0, 0}}
        }
    };
    
    std::vector<DialogTextConfig> partsTexts = {
        DialogTextConfig{SDL_Rect{28, 30, 193, 10}, "Spare parts", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    
    std::vector<SDL_Rect> partsLines = {
        {40, 39, 240, 1},
    };
    
    DialogConfig partsDialogConfig;
    partsDialogConfig.bounds = partsDialogRect;
    partsDialogConfig.backgroundColor = {0, 208, 0, 255};
    partsDialogConfig.borderColor = {0, 111, 239, 255};
    partsDialogConfig.buttons = partsButtons;
    partsDialogConfig.texts = partsTexts;
    partsDialogConfig.lines = partsLines;
    partsDialogConfig.contentAreas = std::vector<SDL_Rect>{topRect, bottomRect};
    partsDialogConfig.font = state->GetAssets().GetMainFont();
    partsDialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: // Change
                LOG_INFO("Change part selected");
                break;
            case 1: // Forget it
                LOG_INFO("Forget it selected");
                // Switch back to overview
                SwitchToState(ENGINE_BAY_OVERVIEW);
                break;
        }
    };
    
    ListDialogConfig listConfig;
    listConfig.labels = partsLabels;
    listConfig.listBoxRect = partsListRect;
    listConfig.selectedItemRect = selectedPartRect;
    listConfig.normalTextColor = {0xFF, 0xFF, 0xFF, 0xFF};
    listConfig.selectedTextColor = {0xFF, 0xFF, 0x00, 0xFF};
    listConfig.scrollButtons = {
        scrollTopRect,
        scrollUpRect,
        scrollDownRect,
        scrollBottomRect
    };
    listConfig.onItemSelected = [](int selectedIndex) {
        LOG_INFO("Parts dialog: Selected part index=%d", selectedIndex);
    };
    
    partsDialog = std::make_unique<ListDialog>(partsDialogConfig, listConfig);
}