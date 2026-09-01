#include "state.h"
#include "engine_bay.h"
#include "animated_bitmap.h"

const int buttonWidth = 39;
const int buttonHeight = 9;

// Which lib.p body bitmap represents each part. Carb is confirmed
// (car_builder.cpp's CreateCarburetorPart uses the exact same mapping).
// Engine is inferred by displacement tier - only 3 lib.p engine bodies exist
// (V6/V8/V8_BIG) for 9 CarEngine_t values across 3 vendors, and the body art
// isn't vendor-specific elsewhere (transmission, manifold, carb all reuse one
// bitmap across GM/Ford/Chrysler too) - flag if a particular engine shows the
// wrong body.
AmigaLibAsset EngineBodyAsset(CarEngine_t type) {
    switch (type) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            return ENGINE_V6;
        case CarEngineGM_V8_282:
        case CarEngineFord_V8_292:
        case CarEngineChrysler_V8_285:
            return ENGINE_V8;
        case CarEngineGM_V8_327:
        case CarEngineFord_V8_351:
        case CarEngineChrysler_V8_340:
            return ENGINE_V8_BIG;
    }
    return ENGINE_V8;
}

// Reconstructed from the actual lib.p art (RodLegacyBins/amiga/SR_disk2/lib.p,
// indices 328-333), not confirmed against the game's own class->asset table
// (no Ghidra symbols found for that code path) - flag if a manifold shows
// the wrong body. By mount-flange shape: MANIFOLD1/328 and MANIFOLD3/330 show
// two/one mounts respectively that don't match their old assignments here
// (the previous mapping had 2-barrel showing 328's dual-mount flange, which
// is what prompted this fix); MANIFOLD6/333 shows three mounts (triple);
// MANIFOLD4/331 shows a single narrow 2-bolt flange (2-barrel). Racing has no
// distinct art of its own - it reuses 4-barrel's flange (330), since
// GetCompatibleCarburetors(MANIFOLD_RACING) requires a 4-barrel-shaped carb
// and no second wide-flange asset exists. MANIFOLD2/329 and MANIFOLD5/332
// are near-duplicates of 328 and 331 and appear unused.
AmigaLibAsset ManifoldBodyAsset(ManifoldType type) {
    switch (type) {
        case MANIFOLD_2BARREL: return ENGINE_MANIFOLD4;
        case MANIFOLD_4BARREL: return ENGINE_MANIFOLD3;
        case MANIFOLD_DUAL_QUAD: return ENGINE_MANIFOLD1;
        case MANIFOLD_TRIPLE_2BARREL: return ENGINE_MANIFOLD6;
        case MANIFOLD_RACING: return ENGINE_MANIFOLD3;
    }
    return ENGINE_MANIFOLD4;
}

// A car loaded from its SR_data spec (i.e. never had this part explicitly
// fitted - see car.h's specEngineClass/specManifoldClass comment) never gets
// a concrete manifold/engine Part; GetManifold()/GetEngine() return a
// default-constructed Part/EnginePart whose fields are garbage for this
// purpose (Part's union field reads back as whichever enum bit-pattern
// TRANS_4SPD_UNIVERSAL happens to be). car.cpp's CarSetupFromCar already
// handles this correctly for scoring - mirror it here for display. Carb has
// no such gap: ApplySpec always calls SetCarburator with a real part.
static ManifoldType ManifoldTypeFromSpecClass(int cls) {
    switch (cls) {
        case 0: return MANIFOLD_2BARREL;
        case 1: return MANIFOLD_4BARREL;
        case 2: return MANIFOLD_DUAL_QUAD;
        case 3: return MANIFOLD_TRIPLE_2BARREL;
        case 4: return MANIFOLD_RACING;
    }
    return MANIFOLD_2BARREL;
}

ManifoldType ResolveManifoldType(const Car& car) {
    if (car.GetSpecManifoldClass() >= 0) return ManifoldTypeFromSpecClass(car.GetSpecManifoldClass());
    return car.GetManifold().partType.manifoldType;
}

// Unlike manifold, the raw SR_data engine class (0..3) has no confirmed
// mapping to a specific CarEngine_t - it's a scoring tier, not a documented
// body-shape selector (docs/formats/sr_data.md just calls it "engine class
// 0..3"), and CarSetupFromCar's own derivation collapses V8/V8_BIG to the
// same scoring class in the other direction, so it can't be inverted either.
// This is a guess (weakest->V6, strongest->V8_BIG): flag if a spec-based
// car's engine body looks wrong.
CarEngine_t ResolveEngineType(const Car& car) {
    if (car.GetSpecEngineClass() >= 0) {
        switch (car.GetSpecEngineClass()) {
            case 0: return CarEngineGM_V6;
            case 1:
            case 2: return CarEngineGM_V8_282;
            default: return CarEngineGM_V8_327;
        }
    }
    return car.GetEngine().engineType;
}

AmigaLibAsset CarbBodyAsset(CarburetorType type) {
    switch (type) {
        case CARB_2BARREL: return ENGINE_CARB_2BARREL;
        case CARB_4BARREL: return ENGINE_CARB_4BARREL;
        case CARB_4BARREL_RACING: return ENGINE_CARB_4BARREL_RACING;
    }
    return ENGINE_CARB_2BARREL;
}

EngineBay::EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback,
                      ZoneClickCallback zoneClickCallback)
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

    // Part-swap screws (engine mount x1, manifold x2, carb x1) - see
    // garage/garage_engine_bay_install.cpp for the click-driven state
    // machine (same idea as the transmission window's screws, but frame-
    // select rather than a growing crop - see the screw bitmap setup
    // below). Positions are placeholders (borrowed from the existing
    // static part-bitmap rects below) pending eyeballing against the
    // running game.
    SDL_Rect engineScrewRect = {126, 165, 16, 8};
    SDL_Rect manifoldLeftScrewRect = {90, 48, 16, 8};
    SDL_Rect manifoldRightScrewRect = {170, 48, 16, 8};
    SDL_Rect carbScrewRect = {130, 32, 16, 8};
    Zone engineScrewZone("screw_0", "Screw", SDL_Rect{engineScrewRect.x - 4, engineScrewRect.y - 4, 24, 16});
    Zone manifoldLeftScrewZone("screw_1", "Screw", SDL_Rect{manifoldLeftScrewRect.x - 4, manifoldLeftScrewRect.y - 4, 24, 16});
    Zone manifoldRightScrewZone("screw_2", "Screw", SDL_Rect{manifoldRightScrewRect.x - 4, manifoldRightScrewRect.y - 4, 24, 16});
    Zone carbScrewZone("screw_3", "Screw", SDL_Rect{carbScrewRect.x - 4, carbScrewRect.y - 4, 24, 16});

    // Same rects as the body bitmaps below - click to open the spare-parts
    // picker once a slot is fully unscrewed (Garage ignores the click
    // otherwise). This is also the only way back in after "Forget it".
    Zone enginePartZone("part_0", "Engine", SDL_Rect{62, 51, 136, 112});
    Zone manifoldPartZone("part_1", "Manifold", SDL_Rect{93, 52, 88, 31});
    Zone carbPartZone("part_2", "Carburetor", SDL_Rect{116, 35, 32, 29});

    // The engine's ground wire has to be unplugged (as well as its screw
    // backed out) before the engine can come out - same rect as the
    // engine_wire bitmap below.
    SDL_Rect wireRect = {162, 149, 40, 21};
    Zone wireZone("wire_0", "Wire", wireRect);

    std::vector<Zone> engineDialogZones = {
        tuneZone, partsZone, doneZone,
        engineScrewZone, manifoldLeftScrewZone, manifoldRightScrewZone, carbScrewZone,
        enginePartZone, manifoldPartZone, carbPartZone, wireZone
    };
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
    overviewConfig.onZoneClick = zoneClickCallback;

    // Overview dialog bitmaps
    overviewConfig.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    overviewConfig.bitmaps["button_sidebar"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS}),
        {225, 0, 71, 190}, 1
    };
    // Engine/manifold/carb bitmaps track the current car's actual parts
    // (previously always drew the same placeholder V6/MANIFOLD5/2-barrel
    // regardless of what was installed). Swapping updates these via
    // GetBitmap() rather than rebuilding the dialog - see
    // garage/garage_engine_bay_install.cpp.
    Car* currentCar = state.GetCurrentCar();
    overviewConfig.bitmaps["engine_body"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, EngineBodyAsset(ResolveEngineType(*currentCar))}),
        {62, 51, 136, 112}, 2
    };
    overviewConfig.bitmaps["manifold_body"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ManifoldBodyAsset(ResolveManifoldType(*currentCar))}),
        {93, 52, 88, 31}, 2
    };
    overviewConfig.bitmaps["carb_body"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, CarbBodyAsset(currentCar->GetCarburator().partType.carburetorType)}),
        {116, 35, 32, 29}, 4
    };
    // "engine_wire" (not "..._attached") since garage_engine_bay_install.cpp
    // toggles this between ENGINE_WIRE_ATTACHED/ENGINE_WIRE_DETACHED.
    overviewConfig.bitmaps["engine_wire"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED}),
        {162, 149, 40, 21}, 3
    };
    overviewConfig.bitmaps["bolt1"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT}),
        {162, 180, 8, 4}, 3,
        {0, 0, 8, 4}  // crop to just the bolt head (rows 0-3 of the 8x12 asset)
    };

    // Part-mount screws: independent ENGINE_BOLT copies (not a shared
    // pointer - same reasoning as the transmission screws, so hiding one
    // via SetVisible can't affect the others), turn-state selected via a
    // row crop (ENGINE_BOLT is a real 3-frame sheet - one 8x4 frame per
    // row - unlike the transmission's ENGINE_TRANSMISSION_BOLT which was a
    // single growing image, so no push-out/rotation here, just frame select).
    Bitmap* boltSrc = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT});
    SDL_Rect screwRects[4] = {engineScrewRect, manifoldLeftScrewRect, manifoldRightScrewRect, carbScrewRect};
    for (int i = 0; i < 4; i++) {
        screwBitmap[i] = std::make_unique<Bitmap>(*boltSrc);
        SDL_Rect natural = screwBitmap[i]->GetPosition();
        screwNaturalSize[i] = {natural.w, natural.h};
        SDL_Rect frameCrop = {0, 0, natural.w, natural.h / 3};  // state 0 = seated
        overviewConfig.bitmaps["screw_" + std::to_string(i)] = {
            screwBitmap[i].get(), {screwRects[i].x, screwRects[i].y, frameCrop.w, frameCrop.h}, 5, frameCrop
        };
    }

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

void EngineBay::RefreshPartsForCurrentCar() {
    Car* car = state->GetCurrentCar();
    if (!car || !engineDialog) return;

    if (DialogBitmap* db = engineDialog->GetBitmap("engine_body")) {
        db->bitmap = state->GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, EngineBodyAsset(ResolveEngineType(*car))});
    }
    if (DialogBitmap* db = engineDialog->GetBitmap("manifold_body")) {
        db->bitmap = state->GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, ManifoldBodyAsset(ResolveManifoldType(*car))});
    }
    if (DialogBitmap* db = engineDialog->GetBitmap("carb_body")) {
        db->bitmap = state->GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, CarbBodyAsset(car->GetCarburator().partType.carburetorType)});
    }
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