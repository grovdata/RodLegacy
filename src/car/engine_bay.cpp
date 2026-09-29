#include "core/state.h"
#include "car/engine_bay.h"
#include <cstdlib>
#include "car/engine_bay_layout.h"
#include "gfx/animated_bitmap.h"

const int buttonWidth = 39;
const int buttonHeight = 9;

// Engine art is inferred by tier: only 3 bodies exist for 9 engines, and no part art is vendor-specific.
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

// Asset ids are lib.p index + 1 in the part table, and its class order has triple 2-barrel at 2, dual quad at 3.
// MANIFOLD5 is the narrow 2-barrel body, substituted for MANIFOLD4 on a V6.
AmigaLibAsset ManifoldBodyAsset(ManifoldType type, CarEngine_t engine) {
    switch (type) {
        case MANIFOLD_2BARREL:
            return ManifoldBodyIsNarrow(type, engine) ? ENGINE_MANIFOLD5 : ENGINE_MANIFOLD4;
        case MANIFOLD_4BARREL: return ENGINE_MANIFOLD3;
        case MANIFOLD_DUAL_QUAD: return ENGINE_MANIFOLD2;
        case MANIFOLD_TRIPLE_2BARREL: return ENGINE_MANIFOLD6;
        case MANIFOLD_RACING: return ENGINE_MANIFOLD1;
    }
    return ENGINE_MANIFOLD4;
}

AmigaLibAsset CarbBodyAsset(CarburetorType type) {
    switch (type) {
        case CARB_2BARREL: return ENGINE_CARB_2BARREL;
        case CARB_4BARREL: return ENGINE_CARB_4BARREL;
        case CARB_4BARREL_RACING: return ENGINE_CARB_4BARREL_RACING;
    }
    return ENGINE_CARB_2BARREL;
}

namespace {
// At the art's own size: scaling wrecks the 1-bit lettering.
SDL_Rect SidebarRect(Bitmap* sidebar) {
    const int x = 225;
    if (!sidebar) return {x, 0, 71, 190};
    const SDL_Rect natural = sidebar->GetNaturalSize();
    return {x, 0, natural.w, natural.h};
}
}

EngineBay::EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback,
                      ZoneClickCallback zoneClickCallback)
    : state(&state), currentState(ENGINE_BAY_OVERVIEW) {

    this->tuneLevel = tuneLevel;
    this->engineSpeed = tuneLevel;
    
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
    // From the original's blit records; by-eye values did not line up.
    SDL_Rect beltAnimationRect = {65, 37, 86, 53};
    SDL_Rect leftPistonRect = {53, 68, 42, 55};
    SDL_Rect rightPistonRect = {134, 68, 41, 54};
    SDL_Rect crankRect = {79, 122, 71, 33};
    SDL_Rect flywheelRect = {148, 122, 43, 34};
    SDL_Rect camRect = {95, 109, 39, 14};
    SDL_Rect tuneKnobRect = {124, 22, 16, 10};


    Zone tuneZone("tune", "Tune", tuneRect);
    Zone partsZone("parts", "Parts", partsRect);
    Zone doneZone("done", "Done", doneRect);
    Zone retardZone("retard", "Retard", retardRect);
    Zone advanceZone("advance", "Advance", advanceRect);
    Zone doneTuneZone("done_tune", "Done", partsRect);

    auto& assets = state.GetAssets();
    Car* currentCar = state.GetCurrentCar();
    CarEngine_t engineType = currentCar->GetEngineType();
    ManifoldType manifoldType = currentCar->GetManifoldType();
    CarburetorType carbType = currentCar->GetCarburator().partType.carburetorType;

    Bitmap* engineBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, EngineBodyAsset(engineType)});
    Bitmap* manifoldBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ManifoldBodyAsset(manifoldType, engineType)});
    Bitmap* carbBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, CarbBodyAsset(carbType)});
    Bitmap* wireBmp = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED});

    SDL_Rect engineBodyRect = EngineBodyRect(engineBmp->GetNaturalSize(), engineType);
    SDL_Rect manifoldBodyRect = ManifoldBodyRect(manifoldBmp->GetNaturalSize(), manifoldType);
    SDL_Rect wireRect = EngineWireRect(wireBmp->GetNaturalSize(), engineType);

    SDL_Point screwAnchor[kEngineBayScrewCount];
    for (int i = 0; i < kEngineBayScrewCount; i++) {
        screwAnchor[i] = EngineBayScrewAnchor(i, engineType, manifoldType, carbType);
    }

    // Click to open the picker once a slot is unscrewed; also the only way back in after "Forget it".
    Zone enginePartZone("part_0", "Engine", engineBodyRect);
    Zone manifoldPartZone("part_1", "Manifold", manifoldBodyRect);

    Zone wireZone("wire_0", "Wire", wireRect);

    // Hit-testing takes the first containing zone, so the smallest go first.
    std::vector<Zone> engineDialogZones = {tuneZone, partsZone, doneZone};
    for (int i = 0; i < kEngineBayScrewCount; i++) {
        engineDialogZones.emplace_back("screw_" + std::to_string(i), "Screw",
                                       EngineBayScrewZone(screwAnchor[i]));
    }
    engineDialogZones.push_back(wireZone);
    // RefreshPartsForCurrentCar disables the mounts this manifold lacks.
    for (int i = 0; i < kMaxCarburetors; i++) {
        engineDialogZones.emplace_back("part_" + std::to_string(2 + i), "Carburetor",
                                       CarbBodyRect(carbBmp->GetNaturalSize(), manifoldType, engineType, i),
                                       i < CarburetorsPerManifold(manifoldType));
    }
    engineDialogZones.push_back(manifoldPartZone);
    engineDialogZones.push_back(enginePartZone);
    std::vector<Zone> tuneDialogZones = {retardZone, advanceZone, doneTuneZone};

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
            .bitmapConfig = {buttonBitmap, {18, 150, buttonWidth, buttonHeight}},
            .cancelChoice = true
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
            .defaultChoice = true,
            .index = 2,
            .textColor = ButtonDefaultTextColor,
            .fillColor = ButtonDefaultFillColor,
            .borderColor = ButtonDefaultBorderColor,
            .invisible = true,
            .bitmapConfig = {buttonBitmapDone, {18, 150, buttonWidth, buttonHeight}},
            .cancelChoice = true
        }
    };

    LOG_INFO("Tuning buttons created:");
    for (size_t i = 0; i < tuningButtons.size(); i++) {
        LOG_INFO("  Button %zu: %s (index: %d)", i, tuningButtons[i].text.c_str(), tuningButtons[i].index);
    }

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
    
    auto tuningButtonCallbackWrapper = [this, buttonCallback](const int buttonIndex) {
        LOG_INFO("=== TUNING BUTTON CALLBACK ===");
        LOG_INFO("tunelevel %d, buttonIndex: %d", this->tuneLevel, buttonIndex);
        if (buttonIndex == 0) { // Retard button
            LOG_INFO("Retard button pressed - decreasing tune level");
            SetKnobPosition(this->tuneLevel - 1);
        } else if (buttonIndex == 1) { // Advance button
            LOG_INFO("Advance button pressed - increasing tune level");
            SetKnobPosition(this->tuneLevel + 1);
        } else if (buttonIndex == 2) { // Done button
            LOG_INFO("Done button pressed - switching back to overview");
            SwitchToState(ENGINE_BAY_OVERVIEW);
            LOG_INFO("Current state after switch: %d", this->currentState);
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

    overviewConfig.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    // At its own size (72x189); squeezing it jagged the labels.
    overviewConfig.bitmaps["button_sidebar"] = {
        buttonBitmap, SidebarRect(buttonBitmap), 1
    };
    overviewConfig.bitmaps["engine_body"] = {engineBmp, engineBodyRect, 2};
    overviewConfig.bitmaps["manifold_body"] = {manifoldBmp, manifoldBodyRect, 2};
    for (int i = 0; i < kMaxCarburetors; i++) {
        carbBitmap[i] = std::make_unique<Bitmap>(*carbBmp);
        carbBitmap[i]->SetVisible(currentCar->HasCarburetor(i));
        overviewConfig.bitmaps["carb_body_" + std::to_string(i)] = {
            carbBitmap[i].get(),
            CarbBodyRect(carbBmp->GetNaturalSize(), manifoldType, engineType, i), 4
        };
    }
    // Toggled between ENGINE_WIRE_ATTACHED/ENGINE_WIRE_DETACHED.
    overviewConfig.bitmaps["engine_wire"] = {wireBmp, wireRect, 3};

    // Independent copies, so hiding one screw can't affect the others.
    Bitmap* boltSrc = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT});
    for (int i = 0; i < kEngineBayScrewCount; i++) {
        screwBitmap[i] = std::make_unique<Bitmap>(*boltSrc);
        screwBitmap[i]->SetVisible(i < 3 || currentCar->HasCarburetor(i - 3));
        SDL_Rect natural = screwBitmap[i]->GetNaturalSize();
        screwNaturalSize[i] = {natural.w, natural.h};
        SDL_Rect frameCrop = {0, 0, natural.w, natural.h / 3};  // state 0 = seated
        overviewConfig.bitmaps["screw_" + std::to_string(i)] = {
            screwBitmap[i].get(),
            {screwAnchor[i].x, screwAnchor[i].y - frameCrop.h, frameCrop.w, frameCrop.h},
            5, frameCrop
        };
    }

    engineDialog = new Dialog(overviewConfig);

    DialogConfig tuningConfig = overviewConfig;
    
    tuningConfig.buttons = tuningButtons;
    tuningConfig.zones = tuneDialogZones;
    tuningConfig.onButtonPressed = tuningButtonCallbackWrapper;
    
    tuningConfig.bitmaps.clear();
    tuningConfig.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect, 0
    };
    // Same 1:1 rule.
    tuningConfig.bitmaps["button_sidebar_done"] = {
        buttonBitmapDone, SidebarRect(buttonBitmapDone), 1
    };
    tuningConfig.bitmaps["tuning_gadget"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNE_GADGET}),
        tuningGadgetRect, 3
    };

    // Above the window frame, below everything that animates.
    tuningConfig.bitmaps["engine_backdrop"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,ENGINE_TUNING_SCREEN}),
        engineBackdropRect, 2
    };

    Bitmap* engineTuneAnimationBelt = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BELT_ANIMATION});
    AnimatedBitmap* engineTuneAnimation = new AnimatedBitmap(engineTuneAnimationBelt, beltAnimationRect);
    engineTuneAnimation->SetAnimationFromInfo(GetSpriteAnimationInfo(ENGINE_BELT_ANIMATION));
    engineTuneAnimation->Play();

    tuningConfig.bitmaps["engine_tune_animation"] = {
        engineTuneAnimation,
        beltAnimationRect,
        5
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
    
    InitPartsDialog();
    
    SetKnobPosition(tuneLevel);
}

EngineBay::~EngineBay() {
    // The dialogs are owned by the Garage. Stop the drone or it outlives the screen.
    UpdateBayDrone(false);
}

// Play the ignition once, then hold the ordinary engine loop as the idle.
void EngineBay::UpdateBayDrone(bool wanted) {
    AudioManager* audio = state ? state->GetAssets().GetAudioManager() : nullptr;
    if (!audio) {
        return;
    }
    // Only the tuning window runs the drone; the level also moves from the parts view.
    if (!wanted || currentState != ENGINE_BAY_TUNING) {
        if (bayDronePlaying) {
            audio->StopEngineBayDrone();
            bayDronePlaying = false;
        }
        return;
    }
    const int off = tuneLevel - kPerfectTuneLevel;
    if (bayDronePlaying) {
        audio->SetEngineBayTune(off);
    } else {
        audio->StartEngineBayDrone(off);
        bayDronePlaying = true;
    }
}

void EngineBay::SwitchToState(EngineBayState newState) {
    if (currentState == newState) {
        return; // Already in this state
    }

    LOG_INFO("EngineBay::SwitchToState: %d -> %d", currentState, newState);
    currentState = newState;
    UpdateBayDrone(currentState == ENGINE_BAY_TUNING);
}

void EngineBay::ResetToOverview() {
    currentState = ENGINE_BAY_OVERVIEW;
    UpdateBayDrone(false);
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

// The bay shows the car's own assembly state.
bool EngineBay::IsPartFitted(int partSlot) {
    Car* car = state->GetCurrentCar();
    if (!car) return false;
    if (partSlot == 0) return car->HasEngine();
    if (partSlot == 1) return car->HasManifold();
    return car->HasCarburetor(partSlot - 2);
}

// A 2-barrel's extra carb mounts clamp onto a real one, so leaving them on parks a phantom carburetor.
bool EngineBay::IsPartSlotUsed(int partSlot) {
    Car* car = state->GetCurrentCar();
    if (partSlot < 2 || !car) return true;
    return partSlot - 2 < car->GetCarburetorSlots();
}

void EngineBay::OnDialogShown() {
    // Show() turns every bitmap on; Garage fixes what's fitted, this only hides impossible mounts.
    if (!engineDialog) return;
    for (int slot = 2; slot < kEngineBayPartSlots; slot++) {
        if (IsPartSlotUsed(slot)) continue;
        SetPartVisible(slot, false);
        int screw = GetFirstScrew(slot);
        if (screwBitmap[screw]) screwBitmap[screw]->SetVisible(false);
    }
}

std::string EngineBay::GetPartBitmapId(int partSlot) {
    if (partSlot == 0) return "engine_body";
    if (partSlot == 1) return "manifold_body";
    return "carb_body_" + std::to_string(partSlot - 2);
}

SDL_Rect EngineBay::GetPartRect(int partSlot) {
    Car* car = state->GetCurrentCar();
    if (!car || !engineDialog) return {0, 0, 0, 0};
    DialogBitmap* db = engineDialog->GetBitmap(GetPartBitmapId(partSlot));
    if (!db || !db->bitmap) return {0, 0, 0, 0};
    SDL_Rect asset = db->bitmap->GetNaturalSize();
    switch (partSlot) {
        case 0: return EngineBodyRect(asset, car->GetEngineType());
        case 1: return ManifoldBodyRect(asset, car->GetManifoldType());
        default:
            return CarbBodyRect(asset, car->GetManifoldType(), car->GetEngineType(), partSlot - 2);
    }
}

void EngineBay::SetPartVisible(int partSlot, bool visible) {
    if (!engineDialog) return;
    DialogBitmap* db = engineDialog->GetBitmap(GetPartBitmapId(partSlot));
    if (db && db->bitmap) db->bitmap->SetVisible(visible && IsPartSlotUsed(partSlot));
}

void EngineBay::SetPartAsset(int partSlot, AmigaLibAsset asset) {
    if (!engineDialog) return;
    DialogBitmap* db = engineDialog->GetBitmap(GetPartBitmapId(partSlot));
    if (!db) return;
    Bitmap* src = state->GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    if (partSlot >= 2) {
        // Mounts often share one asset, so each needs its own copy.
        carbBitmap[partSlot - 2] = std::make_unique<Bitmap>(*src);
        db->bitmap = carbBitmap[partSlot - 2].get();
    } else {
        db->bitmap = src;
    }
}

SDL_Point EngineBay::GetScrewAnchor(int screwIndex) {
    Car* car = state->GetCurrentCar();
    if (!car) return {0, 0};
    return EngineBayScrewAnchor(screwIndex, car->GetEngineType(), car->GetManifoldType(),
                                car->GetCarburator().partType.carburetorType);
}

int EngineBay::GetFirstScrew(int partSlot) {
    return partSlot == 0 ? 0 : (partSlot == 1 ? 1 : 1 + partSlot);
}

int EngineBay::GetScrewCount(int partSlot) {
    return partSlot == 1 ? 2 : 1;
}

void EngineBay::RefreshPartsForCurrentCar() {
    Car* car = state->GetCurrentCar();
    if (!car || !engineDialog) return;

    // The tuning window keeps its own copy of the level.
    SetKnobPosition(car->GetTuneLevel());

    CarEngine_t engineType = car->GetEngineType();
    int carbSlots = car->GetCarburetorSlots();
    for (int slot = 0; slot < kEngineBayPartSlots; slot++) {
        AmigaLibAsset asset = slot == 0   ? EngineBodyAsset(engineType)
                              : slot == 1 ? ManifoldBodyAsset(car->GetManifoldType(), engineType)
                                          : CarbBodyAsset(car->GetCarburetor(slot - 2).partType.carburetorType);
        SetPartAsset(slot, asset);
        SetPartVisible(slot, IsPartFitted(slot));

        DialogBitmap* db = engineDialog->GetBitmap(GetPartBitmapId(slot));
        if (!db) continue;
        db->rect = GetPartRect(slot);
        db->sourceRect = {0, 0, 0, 0};
        if (Zone* z = engineDialog->GetZone("part_" + std::to_string(slot))) {
            z->SetRect(db->rect);
            z->SetEnabled(slot < 2 || slot - 2 < carbSlots);
        }
    }

    if (DialogBitmap* db = engineDialog->GetBitmap("engine_wire"); db && db->bitmap) {
        db->rect = EngineWireRect(db->bitmap->GetNaturalSize(), engineType);
        db->sourceRect = {0, 0, 0, 0};
        if (Zone* z = engineDialog->GetZone("wire_0")) z->SetRect(db->rect);
    }

    for (int i = 0; i < kEngineBayScrewCount; i++) {
        bool used = i < 3 || i - 3 < carbSlots;
        SDL_Point anchor = GetScrewAnchor(i);
        if (Zone* z = engineDialog->GetZone("screw_" + std::to_string(i))) {
            z->SetRect(EngineBayScrewZone(anchor));
            z->SetEnabled(used);
        }
        DialogBitmap* db = engineDialog->GetBitmap("screw_" + std::to_string(i));
        if (!db) continue;
        screwBitmap[i]->SetVisible(used && IsPartFitted(i < 3 ? i == 0 ? 0 : 1 : i - 1));
        SDL_Rect frameCrop = {0, 0, screwNaturalSize[i].x, screwNaturalSize[i].y / 3};
        db->sourceRect = frameCrop;
        db->rect = {anchor.x, anchor.y - frameCrop.h, frameCrop.w, frameCrop.h};
    }
}

void EngineBay::SetKnobPosition(int level) {
    if (level < 0) level = 0;
    if (level > 12) level = 12;

    // Hooked here: Retard/Advance exist as both buttons and zones.
    if (level != tuneLevel) {
        UpdateBayDrone(true);
    }

    tuneLevel = level;
    engineSpeed = level * 0.0625f;

    if (state->GetCurrentCar()) {
        state->GetCurrentCar()->SetTuneLevel(level);
    }

    // Higher tune level = faster animations.
    const char* animationNames[] = {
        "engine_tune_animation", "left_piston_animation", "right_piston_animation",
        "crank_animation", "flywheel_animation", "cam_animation"
    };
    
    for (const char* name : animationNames) {
        DialogBitmap* bitmap = tuneDialog->GetBitmap(name);
        if (bitmap && bitmap->bitmap) {
            if (AnimatedBitmap* anim = dynamic_cast<AnimatedBitmap*>(bitmap->bitmap)) {
                float frameDuration = 1.0f - engineSpeed;
                anim->SetSpeed(frameDuration);
            }
        }
    }

    // Move the dialog entry's rect: Dialog::Draw re-applies it to the bitmap every frame.
    DialogBitmap* knobBitmap = tuneDialog->GetBitmap("tune_knob");
    if (knobBitmap) {
        // Level 8 is where the indicator's own rect sits (124).
        knobBitmap->rect.x = 108 + level * 2;
    }
}

void EngineBay::InitPartsDialog() {
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
            .bitmapConfig = {nullptr, {0, 0, 0, 0}},
            .cancelChoice = true
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

EngineBayView::EngineBayView(Car& car, State& state, std::function<void()> onClose) {
    auto& assets = state.GetAssets();
    auto lib = [&assets](AmigaLibAsset a) {
        return assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, a});
    };

    CarEngine_t engineType = car.GetEngineType();
    ManifoldType manifoldType = car.GetManifoldType();
    CarburetorType carbType = car.GetCarburator().partType.carburetorType;

    // Same bay rect as the garage, so every position table lands the same, on black.
    SDL_Rect bounds = {24, 0, 200, 189};

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0, 0, 0, 255};
    cfg.borderColor = {0, 0, 0, 255};
    cfg.contentAreas = std::vector<SDL_Rect>{bounds};
    cfg.font = assets.GetMainFont();
    cfg.zones = {Zone("close", "", {0, 0, GAME_WIDTH, GAME_HEIGHT})};
    cfg.onZoneClick = [onClose](Zone&) { if (onClose) onClose(); };
    cfg.onDismiss = onClose;

    // The garage bay's window border (lib.p 288), minus its sidebar.
    cfg.bitmaps["background"] = {lib(ENGINE_WINDOWFRAME), bounds, 0};

    // Only fitted parts get an entry: Show() turns every bitmap visible.
    Bitmap* boltBmp = lib(ENGINE_BOLT);
    SDL_Rect boltNatural = boltBmp->GetNaturalSize();
    SDL_Rect boltCrop = {0, 0, boltNatural.w, boltNatural.h / 3};  // seated frame
    auto addScrew = [&](int screwIndex) {
        SDL_Point anchor = EngineBayScrewAnchor(screwIndex, engineType, manifoldType, carbType);
        cfg.bitmaps["screw_" + std::to_string(screwIndex)] = {
            boltBmp, {anchor.x, anchor.y - boltCrop.h, boltCrop.w, boltCrop.h}, 5, boltCrop};
    };

    if (car.HasEngine()) {
        Bitmap* engineBmp = lib(EngineBodyAsset(engineType));
        Bitmap* wireBmp = lib(ENGINE_WIRE_ATTACHED);
        cfg.bitmaps["engine_body"] = {engineBmp, EngineBodyRect(engineBmp->GetNaturalSize(), engineType), 2};
        cfg.bitmaps["engine_wire"] = {wireBmp, EngineWireRect(wireBmp->GetNaturalSize(), engineType), 3};
        addScrew(0);
    }
    if (car.HasManifold()) {
        Bitmap* manifoldBmp = lib(ManifoldBodyAsset(manifoldType, engineType));
        cfg.bitmaps["manifold_body"] = {manifoldBmp, ManifoldBodyRect(manifoldBmp->GetNaturalSize(), manifoldType), 2};
        addScrew(1);
        addScrew(2);
    }
    // A shared carb asset is fine: nothing is hidden individually.
    Bitmap* carbBmp = lib(CarbBodyAsset(carbType));
    for (int i = 0; i < car.GetCarburetorSlots(); i++) {
        if (!car.HasCarburetor(i)) continue;
        cfg.bitmaps["carb_body_" + std::to_string(i)] = {
            carbBmp, CarbBodyRect(carbBmp->GetNaturalSize(), manifoldType, engineType, i), 4};
        addScrew(3 + i);
    }

    dialog = std::make_unique<Dialog>(cfg);
}