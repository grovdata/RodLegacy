#include "state.h"
#include "engine_bay.h"
#include "animated_bitmap.h"

const int buttonWidth = 39;
const int buttonHeight = 9;

EngineBay::EngineBay(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback) 
    : state(&state), currentState(ENGINE_BAY_OVERVIEW) {
    
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
        {tuneRect, "Tune", nullptr, true, 0, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmap, {18, 31, buttonWidth, buttonHeight}}},
        {partsRect, "Parts", nullptr, false, 1, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmap, {18, 90, buttonWidth, buttonHeight}}},
        {doneRect, "Done", nullptr, false, 2, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmap, {18, 150, buttonWidth, buttonHeight}}}
    };

    std::vector<ButtonConfig> tuningButtons = {
        {retardRect, "Retard", nullptr, true, 0, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmapDone, {98, 9, 10, 11}}},
        {advanceRect, "Advance", nullptr, false, 1, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmapDone, {143, 9, 10, 11}}},
        {doneRect, "Done", nullptr, false, 2, ButtonDefaultTextColor, ButtonDefaultFillColor, ButtonDefaultBorderColor, {buttonBitmapDone, {18, 150, buttonWidth, buttonHeight}}}
    };

    // Common callback setup
    auto buttonCallbackWrapper = [this, buttonCallback](const int buttonIndex) {
        if (buttonIndex == 0) { // Tune button - handled by caller
        }
        if (buttonCallback) {
            buttonCallback(buttonIndex);
        }
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



    tuneDialog = new Dialog(tuningConfig);
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
