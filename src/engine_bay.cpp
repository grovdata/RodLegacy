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


    Zone tuneZone(ZONE_GARAGE_ENGINE_OVERVIEW_TUNE, strdup("Tune"), &tuneRect);
    Zone partsZone(ZONE_GARAGE_ENGINE_OVERVIEW_PARTS, strdup("Parts"), &partsRect);
    Zone doneZone(ZONE_GARAGE_ENGINE_OVERVIEW_DONE, strdup("Done"), &doneRect);
    Zone retardZone(ZONE_GARAGE_ENGINE_OVERVIEW_RETARD, strdup("Retard"), &retardRect);
    Zone advanceZone(ZONE_GARAGE_ENGINE_OVERVIEW_ADVANCE, strdup("Advance"), &advanceRect);

    std::vector<Zone> engineDialogZones = {tuneZone, partsZone, doneZone};
    std::vector<Zone> tuneDialogZones = {retardZone, advanceZone, doneZone};

    // Common button configuration
    Bitmap* buttonBitmap = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS});
    Bitmap* buttonBitmapDone = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS_DONE});
    
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
            .invisible = false,
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
            .invisible = false,
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
            .invisible = false,
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
            .bounds = doneRect,
            .text = "Done",
            .onClick = nullptr,
            .defaultChoice = false,
            .index = 2,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = false,
            .bitmapConfig = {buttonBitmapDone, {18, 150, buttonWidth, buttonHeight}}
        }
    };

    // Debug: Log the tuning button indices
    SDL_Log("Tuning buttons created:");
    for (size_t i = 0; i < tuningButtons.size(); i++) {
        SDL_Log("  Button %zu: %s (index: %d)", i, tuningButtons[i].text.c_str(), tuningButtons[i].index);
    }

    // Common callback setup
    auto buttonCallbackWrapper = [this, buttonCallback](const int buttonIndex) {
        SDL_Log("=== OVERVIEW BUTTON CALLBACK ===");
        SDL_Log("Overview button index: %d", buttonIndex);
        if (buttonIndex == 0) { // Tune button - handled by caller
            SDL_Log("Tune button pressed in overview");
        }
        if (buttonCallback) {
            SDL_Log("Calling original buttonCallback from overview");
            buttonCallback(buttonIndex);
        }
        SDL_Log("=== END OVERVIEW BUTTON CALLBACK ===");
    };
    
    // Tuning-specific callback setup
    auto tuningButtonCallbackWrapper = [this, buttonCallback](const int buttonIndex) {
        SDL_Log("=== TUNING BUTTON CALLBACK ===");
        SDL_Log("tunelevel %d, buttonIndex: %d", this->tuneLevel, buttonIndex);
        if (buttonIndex == 0) { // Retard button
            SDL_Log("Retard button pressed - decreasing tune level");
            SetKnobPosition(this->tuneLevel - 1);
            // Don't call original callback for Retard button
        } else if (buttonIndex == 1) { // Advance button
            SDL_Log("Advance button pressed - increasing tune level");
            SetKnobPosition(this->tuneLevel + 1);
            // Don't call original callback for Advance button
        } else if (buttonIndex == 2) { // Done button
            SDL_Log("Done button pressed - this should dismiss the dialog");
            // Only call original callback for Done button
            if (buttonCallback) {
                SDL_Log("Calling original buttonCallback for Done button");
                buttonCallback(buttonIndex);
            }
        } else {
            SDL_Log("Unknown button index: %d", buttonIndex);
            if (buttonCallback) {
                buttonCallback(buttonIndex);
            }
        }
        SDL_Log("=== END TUNING BUTTON CALLBACK ===");
    };
    
    auto zoneHoverWrapper = zoneHoverCallback ? zoneHoverCallback : [](Zone& zone) {
        SDL_Log("Engine Bay Zone Hover: %s (Zone ID: %d)", zone.GetText(), zone.GetId());
    };

    // Create overview dialog
    DialogConfig overviewConfig;
    overviewConfig.bounds = engineBayRect;
    overviewConfig.backgroundColor = {0, 0, 0, 255};
    overviewConfig.borderColor = {0, 0, 0, 0};
    overviewConfig.buttons = overviewButtons;
    overviewConfig.zones = engineDialogZones;
    overviewConfig.contentAreas = std::vector<SDL_Rect>{engineBayFillRect, sidebarRect, gapRect};
    overviewConfig.font = state.assets.GetMainFont();
    overviewConfig.onButtonPressed = buttonCallbackWrapper;
    overviewConfig.onZoneHover = zoneHoverWrapper;

    // Overview dialog bitmaps
    overviewConfig.bitmaps["background"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    overviewConfig.bitmaps["button_sidebar"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS}),
        {225, 0, 71, 190}, 1
    };
    overviewConfig.bitmaps["engine_test_v6"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_V6}),
        {62, 51, 136, 112}, 2
    };
    overviewConfig.bitmaps["manifold"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_MANIFOLD5}),
        {93, 52, 88, 31}, 2
    };
    overviewConfig.bitmaps["carb2brl"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CARB_2BARREL}),
        {116, 35, 32, 29}, 4
    };
    overviewConfig.bitmaps["engine_wire_attached"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED}),
        {162, 149, 40, 21}, 3
    };
    overviewConfig.bitmaps["bolt1"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT}),
        {162, 180, 8, 12}, 3
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
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    tuningConfig.bitmaps["button_sidebar_done"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS_DONE}),
        {225, 0, 71, 190}, 1
    };
    tuningConfig.bitmaps["tuning_gadget"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNE_GADGET}),
        tuningGadgetRect
    };

    tuningConfig.bitmaps["engine_backdrop"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNING_SCREEN}),
        engineBackdropRect
    };

    // Create animated engine tune bitmap
    Bitmap* engineTuneAnimationBelt = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BELT_ANIMATION});
    AnimatedBitmap* engineTuneAnimation = new AnimatedBitmap(engineTuneAnimationBelt, beltAnimationRect);
    engineTuneAnimation->SetAnimationFrames({{0, 0, 86, 53}, {86, 0, 86, 53}, {0, 53, 86, 53}, {86, 53, 86, 53}});
    engineTuneAnimation->SetSpeed(0.5f);
    engineTuneAnimation->SetLoop(true);
    engineTuneAnimation->Play();

    tuningConfig.bitmaps["engine_tune_animation"] = {
        engineTuneAnimation,
        beltAnimationRect
    };

    Bitmap* leftPiston = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_LEFT_ANIMATION});
    AnimatedBitmap *leftPistonAnimation = new AnimatedBitmap(leftPiston, leftPistonRect);
    // leftPistonAnimation->SetAnimationFrames({
    //     {0, 0, 42, 55},
    //     {42, 0, 42, 55},
    //     {84, 0, 42, 55},
    //     {126, 0, 42, 55},
    //     {0, 55, 42, 55},
    //     {42, 55, 42, 55},
    //     {84, 55, 42, 55},
    //     {126, 55, 42, 55},
    //     }
    //     );
    leftPistonAnimation->SetAnimationFrames(42, 55, 4, 2);
    leftPistonAnimation->SetSpeed(0.5f);
    leftPistonAnimation->SetLoop(true);
    leftPistonAnimation->Play();

    tuningConfig.bitmaps["left_piston_animation"] = {
        leftPistonAnimation,
        leftPistonRect,
        5
    };

    Bitmap* rightPiston = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_RIGHT_ANIMATION});
    AnimatedBitmap *rightPistonAnimation = new AnimatedBitmap(rightPiston, rightPistonRect);
    // rightPistonAnimation->SetAnimationFrames({
    //     {0, 0, 41, 54},
    //     {41, 0, 41, 54},
    //     {83, 0, 41, 54},
    //     {123, 0, 41, 54},
    //     {0, 54, 41, 54},
    //     {41, 54, 41, 54},
    //     {83, 54, 41, 54},
    //     {123, 54, 41, 54},
    //     }
    //     );
    rightPistonAnimation->SetAnimationFrames(41, 54, 4, 2);
    rightPistonAnimation->SetSpeed(0.5f);
    rightPistonAnimation->SetLoop(true);
    rightPistonAnimation->Play();

    tuningConfig.bitmaps["right_piston_animation"] = {
        rightPistonAnimation,
        rightPistonRect,
        5
    };

    Bitmap* crank = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CRANK_ANIMATION});
    AnimatedBitmap *crankAnimation = new AnimatedBitmap(crank, crankRect);
    crankAnimation->SetAnimationFrames(71, 33, 2, 2);
    crankAnimation->SetSpeed(0.5f);
    crankAnimation->SetLoop(true);
    crankAnimation->Play();

    tuningConfig.bitmaps["crank_animation"] = {
        crankAnimation,
        crankRect,
        5
    };

    Bitmap* flywheel = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_FLYWHEEL_ANIMATION});
    AnimatedBitmap *flywheelAnimation = new AnimatedBitmap(flywheel, flywheelRect);
    flywheelAnimation->SetAnimationFrames(43, 34, 2, 2);
    flywheelAnimation->SetSpeed(0.5f);
    flywheelAnimation->SetLoop(true);
    flywheelAnimation->Play();

    tuningConfig.bitmaps["flywheel_animation"] = {
        flywheelAnimation,
        flywheelRect,
        5
    };

    Bitmap* camshaft = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CAM_ANIMATION});
    AnimatedBitmap *camshaftAnimation = new AnimatedBitmap(camshaft, camRect);
    camshaftAnimation->SetAnimationFrames(39, 14, 4, 2);
    camshaftAnimation->SetSpeed(0.5f);
    camshaftAnimation->SetLoop(true);
    camshaftAnimation->Play();

    tuningConfig.bitmaps["cam_animation"] = {
        camshaftAnimation,
        camRect,
        5
    };

    Bitmap* tuneKnob = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_TUNE_INDICATOR});
    tuningConfig.bitmaps["tune_knob"] = {
        tuneKnob,
        tuneKnobRect,
        5
    };


    tuneDialog = new Dialog(tuningConfig);
    
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
    
    SDL_Log("EngineBay::SwitchToState: %d -> %d", currentState, newState);
    currentState = newState;
}

void EngineBay::ResetToOverview() {
    currentState = ENGINE_BAY_OVERVIEW;
    SDL_Log("EngineBay::ResetToOverview: Reset to overview state");
}

Dialog* EngineBay::GetDialog() {
    return (currentState == ENGINE_BAY_TUNING) ? tuneDialog : engineDialog;
}

void EngineBay::OnDialogShown() {
    // No need to restore visibility since we're using separate dialogs
}

void EngineBay::SetKnobPosition(int level) {
    if (level < 0) level = 0;
    if (level > 12) level = 12;
    
    tuneLevel = level;
    engineSpeed = level * 0.0625f;

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