#include "state.h"
#include "engine_bay.h"
#include "animated_bitmap.h"

const int buttonWidth = 39;
const int buttonHeight = 9;

Dialog* EngineBay::CreateTuneDialog(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback) {

    DialogConfig tuneDialogConfig;

    Bitmap* engineTuneAnimationBelt = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BELT_ANIMATION});
    Bitmap* engineTuneAnimationPistonLeft = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_LEFT_ANIMATION});
    Bitmap* engineTuneAnimationPistonRight = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_PISTON_RIGHT_ANIMATION});
    Bitmap* engineTuneAnimationCrank = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CRANK_ANIMATION});
    Bitmap* engineTuneAnimationFlywheel = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_FLYWHEEL_ANIMATION});
    Bitmap* engineTuneAnimationCam = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CAM_ANIMATION});

    AnimatedBitmap* engineTuneAnimation = new AnimatedBitmap(engineTuneAnimationBelt, {0, 0, 85, 53});
    engineTuneAnimation->SetAnimationFrames({{0, 0, 86, 53}, {86, 0, 86, 53}, {0, 53, 86, 53}, {86, 53, 86, 53}});
    engineTuneAnimation->SetSpeed(0.5f);
    engineTuneAnimation->SetLoop(true);
    engineTuneAnimation->Play();

    tuneDialogConfig.bitmaps["engine_tune_animation"] = {
        engineTuneAnimation,
        {0, 0, 85, 53},
        5
    };
}

Dialog* EngineBay::CreateDialog(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback) {

    // sidebar index 295, 324 when tuning
 

    SDL_Rect engineBayRect = {24, 0, 200, 189};
    SDL_Rect engineBayFillRect = {28, 4, 193, 181};
    SDL_Rect sidebarRect = {225, 1, 71, 189};
    SDL_Rect gapRect = {223, 0, 2, 187};

    SDL_Rect tuneRect = {243, 31, buttonWidth, buttonHeight};
    SDL_Rect partsRect = {243, 90, buttonWidth, buttonHeight};
    SDL_Rect doneRect = {243, 150, buttonWidth, buttonHeight};
    
    Zone tuneZone(ZONE_GARAGE_ENGINE_OVERVIEW_TUNE, strdup("Tune"), &tuneRect);
    Zone partsZone(ZONE_GARAGE_ENGINE_OVERVIEW_PARTS, strdup("Parts"), &partsRect);
    Zone doneZone(ZONE_GARAGE_ENGINE_OVERVIEW_DONE, strdup("Done"), &doneRect);

    std::vector<Zone> dialogZones = {
        tuneZone,
        partsZone,
        doneZone,
    };

    // Get the button bitmap
    Bitmap* buttonBitmap = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS});

    ButtonConfig tuneButton = {
        tuneRect,
        "Tune",
        nullptr,
        true,
        0,
        ButtonDefaultTextColor,
        ButtonDefaultFillColor,
        ButtonDefaultBorderColor,
        {buttonBitmap, {18, 31, buttonWidth, buttonHeight}}
    };

    ButtonConfig partsButton = {
        partsRect,
        "Parts",
        nullptr,
        false,
        1,
        ButtonDefaultTextColor,
        ButtonDefaultFillColor,
        ButtonDefaultBorderColor,
        {buttonBitmap, {18, 90, buttonWidth, buttonHeight}}
    };

    ButtonConfig doneButton = {
        doneRect,
        "Done",
        nullptr,
        false,
        2,
        ButtonDefaultTextColor,
        ButtonDefaultFillColor,
        ButtonDefaultBorderColor,
        {buttonBitmap, {18, 150, buttonWidth, buttonHeight}}
    };
    
    std::vector<ButtonConfig> buttons = {
        tuneButton,
        partsButton,
        doneButton,
    };

    
    DialogConfig engineDialogConfig;
    engineDialogConfig.bounds = engineBayRect;
    // TODO: Unsure it should be flat out black
    engineDialogConfig.backgroundColor = {0, 0, 0, 255};
    // engineDialogConfig.backgroundColor = {244, 244, 0, 255};
    engineDialogConfig.borderColor = {0, 0, 0, 0};
    engineDialogConfig.buttons = buttons;
    engineDialogConfig.zones = dialogZones;
    engineDialogConfig.contentAreas = std::vector<SDL_Rect>{engineBayFillRect, sidebarRect, gapRect};
    engineDialogConfig.font = state.assets.GetMainFont();
    engineDialogConfig.bitmaps["background"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME}),
        engineBayRect,
        0
    };
    engineDialogConfig.bitmaps["button_sidebar"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS}),
        {225, 0, 71, 190},
        1,
    };

    engineDialogConfig.bitmaps["engine_test_v6"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_V6}),
        {62, 51, 136, 112},
        2
    };

    engineDialogConfig.bitmaps["manifold"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_MANIFOLD5}),
        {93, 52, 88, 31},
        2
    };

    engineDialogConfig.bitmaps["carb2brl"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_CARB_2BARREL}),
        {116, 35, 32, 29},
        4
    };

    engineDialogConfig.bitmaps["engine_wire_attached"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_ATTACHED}),
        {162, 149, 40, 21},
        3
    };

    engineDialogConfig.bitmaps["bolt1"] = {
        state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_BOLT}),
        {162, 180, 8, 12},
        3
    };

 

    // Set up the callbacks in the config
    engineDialogConfig.onButtonPressed = buttonCallback;
    if (zoneHoverCallback) {
        engineDialogConfig.onZoneHover = zoneHoverCallback;
    } else {
        // Default zone hover behavior - just log it
        engineDialogConfig.onZoneHover = [](Zone& zone) {
            SDL_Log("Engine Bay Zone Hover: %s (Zone ID: %d)", zone.GetText(), zone.GetId());
        };
    }
    
    // Create and return the dialog
    return new Dialog(engineDialogConfig);
}