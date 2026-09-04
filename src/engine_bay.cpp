#include "state.h"
#include "engine_bay.h"
#include "engine_bay_layout.h"
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

// Read straight out of the Amiga part table (A4-0x48ea, 14-byte records,
// asset id at +0x02, manifold class in flag bits 3-5) - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md. The binary's asset ids
// are lib.p index + 1, and its class order is not ManifoldType's: it has
// triple 2-barrel at class 2 and dual quad at class 3. MANIFOLD5/332 is
// unreferenced by any part.
AmigaLibAsset ManifoldBodyAsset(ManifoldType type) {
    switch (type) {
        case MANIFOLD_2BARREL: return ENGINE_MANIFOLD4;
        case MANIFOLD_4BARREL: return ENGINE_MANIFOLD3;
        case MANIFOLD_DUAL_QUAD: return ENGINE_MANIFOLD2;
        case MANIFOLD_TRIPLE_2BARREL: return ENGINE_MANIFOLD6;
        case MANIFOLD_RACING: return ENGINE_MANIFOLD1;
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

    // Every part/screw/wire rect below comes from engine_bay_layout.h, i.e.
    // from the original's own position tables, so they track the fitted
    // parts instead of being fixed boxes.
    auto& assets = state.GetAssets();
    Car* currentCar = state.GetCurrentCar();
    CarEngine_t engineType = ResolveEngineType(*currentCar);
    ManifoldType manifoldType = ResolveManifoldType(*currentCar);
    CarburetorType carbType = currentCar->GetCarburator().partType.carburetorType;

    Bitmap* engineBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, EngineBodyAsset(engineType)});
    Bitmap* manifoldBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ManifoldBodyAsset(manifoldType)});
    Bitmap* carbBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, CarbBodyAsset(carbType)});
    Bitmap* wireBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED});

    SDL_Rect engineBodyRect = EngineBodyRect(engineBmp->GetPosition(), engineType);
    SDL_Rect manifoldBodyRect = ManifoldBodyRect(manifoldBmp->GetPosition(), manifoldType);
    SDL_Rect carbBodyRect = CarbBodyRect(carbBmp->GetPosition(), manifoldType, engineType);
    SDL_Rect wireRect = EngineWireRect(wireBmp->GetPosition(), engineType);

    // Part-swap screws (engine mount x1, manifold x2, carb x1) - see
    // garage/garage_engine_bay_install.cpp for the click-driven state
    // machine.
    SDL_Point screwAnchor[4];
    for (int i = 0; i < 4; i++) {
        screwAnchor[i] = EngineBayScrewAnchor(i, engineType, manifoldType, carbType);
    }
    Zone engineScrewZone("screw_0", "Screw", EngineBayScrewZone(screwAnchor[0]));
    Zone manifoldLeftScrewZone("screw_1", "Screw", EngineBayScrewZone(screwAnchor[1]));
    Zone manifoldRightScrewZone("screw_2", "Screw", EngineBayScrewZone(screwAnchor[2]));
    Zone carbScrewZone("screw_3", "Screw", EngineBayScrewZone(screwAnchor[3]));

    // Same rects as the body bitmaps - click to open the spare-parts picker
    // once a slot is fully unscrewed (Garage ignores the click otherwise).
    // This is also the only way back in after "Forget it".
    Zone enginePartZone("part_0", "Engine", engineBodyRect);
    Zone manifoldPartZone("part_1", "Manifold", manifoldBodyRect);
    Zone carbPartZone("part_2", "Carburetor", carbBodyRect);

    // The engine's ground wire has to be unplugged (as well as its screw
    // backed out) before the engine can come out.
    Zone wireZone("wire_0", "Wire", wireRect);

    // Dialog hit-testing takes the first zone that contains the click, so
    // these run smallest-on-top first: the part bodies overlap each other
    // and swallow the screws and the wire otherwise (the engine body alone
    // covers most of the bay).
    std::vector<Zone> engineDialogZones = {
        tuneZone, partsZone, doneZone,
        engineScrewZone, manifoldLeftScrewZone, manifoldRightScrewZone, carbScrewZone,
        wireZone, carbPartZone, manifoldPartZone, enginePartZone
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
    // Engine/manifold/carb bitmaps track the current car's actual parts.
    // Swapping updates these via GetBitmap() rather than rebuilding the
    // dialog - see garage/garage_engine_bay_install.cpp.
    overviewConfig.bitmaps["engine_body"] = {engineBmp, engineBodyRect, 2};
    overviewConfig.bitmaps["manifold_body"] = {manifoldBmp, manifoldBodyRect, 2};
    overviewConfig.bitmaps["carb_body"] = {carbBmp, carbBodyRect, 4};
    // "engine_wire" (not "..._attached") since garage_engine_bay_install.cpp
    // toggles this between ENGINE_WIRE_ATTACHED/ENGINE_WIRE_DETACHED.
    overviewConfig.bitmaps["engine_wire"] = {wireBmp, wireRect, 3};
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
    Bitmap* boltSrc = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT});
    for (int i = 0; i < 4; i++) {
        screwBitmap[i] = std::make_unique<Bitmap>(*boltSrc);
        SDL_Rect natural = screwBitmap[i]->GetPosition();
        screwNaturalSize[i] = {natural.w, natural.h};
        SDL_Rect frameCrop = {0, 0, natural.w, natural.h / 3};  // state 0 = seated
        overviewConfig.bitmaps["screw_" + std::to_string(i)] = {
            screwBitmap[i].get(),
            {screwAnchor[i].x, screwAnchor[i].y - frameCrop.h, frameCrop.w, frameCrop.h},
            5, frameCrop
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

static const char* kPartBitmapId[3] = {"engine_body", "manifold_body", "carb_body"};

SDL_Rect EngineBay::GetPartRect(int partSlot) {
    Car* car = state->GetCurrentCar();
    if (!car || !engineDialog) return {0, 0, 0, 0};
    DialogBitmap* db = engineDialog->GetBitmap(kPartBitmapId[partSlot]);
    if (!db || !db->bitmap) return {0, 0, 0, 0};
    SDL_Rect asset = db->bitmap->GetPosition();
    switch (partSlot) {
        case 0: return EngineBodyRect(asset, ResolveEngineType(*car));
        case 1: return ManifoldBodyRect(asset, ResolveManifoldType(*car));
        default: return CarbBodyRect(asset, ResolveManifoldType(*car), ResolveEngineType(*car));
    }
}

SDL_Point EngineBay::GetScrewAnchor(int screwIndex) {
    Car* car = state->GetCurrentCar();
    if (!car) return {0, 0};
    return EngineBayScrewAnchor(screwIndex, ResolveEngineType(*car), ResolveManifoldType(*car),
                                car->GetCarburator().partType.carburetorType);
}

void EngineBay::RefreshPartsForCurrentCar() {
    Car* car = state->GetCurrentCar();
    if (!car || !engineDialog) return;

    CarEngine_t engineType = ResolveEngineType(*car);
    AmigaLibAsset bodyAsset[3] = {
        EngineBodyAsset(engineType),
        ManifoldBodyAsset(ResolveManifoldType(*car)),
        CarbBodyAsset(car->GetCarburator().partType.carburetorType)
    };
    for (int i = 0; i < 3; i++) {
        DialogBitmap* db = engineDialog->GetBitmap(kPartBitmapId[i]);
        if (!db) continue;
        db->bitmap = state->GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, bodyAsset[i]});
        db->rect = GetPartRect(i);
        db->sourceRect = {0, 0, 0, 0};
        if (Zone* z = engineDialog->GetZone("part_" + std::to_string(i))) z->SetRect(db->rect);
    }

    if (DialogBitmap* db = engineDialog->GetBitmap("engine_wire"); db && db->bitmap) {
        db->rect = EngineWireRect(db->bitmap->GetPosition(), engineType);
        db->sourceRect = {0, 0, 0, 0};
        if (Zone* z = engineDialog->GetZone("wire_0")) z->SetRect(db->rect);
    }

    // A car swap always arrives fully assembled, so the screws go back to
    // their seated frame at the new mount points.
    for (int i = 0; i < 4; i++) {
        SDL_Point anchor = GetScrewAnchor(i);
        if (Zone* z = engineDialog->GetZone("screw_" + std::to_string(i))) {
            z->SetRect(EngineBayScrewZone(anchor));
        }
        DialogBitmap* db = engineDialog->GetBitmap("screw_" + std::to_string(i));
        if (!db) continue;
        SDL_Rect frameCrop = {0, 0, screwNaturalSize[i].x, screwNaturalSize[i].y / 3};
        db->sourceRect = frameCrop;
        db->rect = {anchor.x, anchor.y - frameCrop.h, frameCrop.w, frameCrop.h};
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