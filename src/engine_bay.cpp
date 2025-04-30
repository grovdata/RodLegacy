#include "state.h"
#include "engine_bay.h"

Dialog* EngineBay::CreateDialog(State& state, ButtonCallback buttonCallback) {

    // sidebar index 295, 324 when tuning
 
    const int buttonWidth = 39;
    const int buttonHeight = 9;

    SDL_Rect engineBayRect = {24, 0, 200, 189};
    SDL_Rect engineBayFillRect = {28, 4, 193, 181};
    SDL_Rect sidebarRect = {225, 1, 71, 189};
    SDL_Rect gapRect = {223, 0, 2, 187};

    SDL_Rect tuneRect = {243, 31, buttonWidth, buttonHeight};
    SDL_Rect partsRect = {243, 90, buttonWidth, buttonHeight};
    SDL_Rect doneRect = {243, 150, buttonWidth, buttonHeight};
    
    Zone tuneZone(ZONE_GARAGE_MENU_SAVE_GAME, strdup("Tune"), &tuneRect);
    Zone partsZone(ZONE_GARAGE_MENU_OLD_GAME, strdup("Parts"), &partsRect);
    Zone doneZone(ZONE_GARAGE_MENU_NEW_GAME, strdup("Done"), &doneRect);

    vector<Zone> dialogZones = {
        tuneZone,
        partsZone,
        doneZone,
    };

    ButtonConfig tuneButton = {tuneRect, "Tune", nullptr, true, 0};
    ButtonConfig partsButton = {partsRect, "Parts", nullptr, false, 1};
    ButtonConfig doneButton = {doneRect, "Done", nullptr, false, 2};
    
    vector<ButtonConfig> buttons = {
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
    engineDialogConfig.contentAreas = vector<SDL_Rect>{engineBayFillRect, sidebarRect, gapRect};
    engineDialogConfig.font = state.assets.GetMainFont();
    engineDialogConfig.useBackgroundImage = true;
    engineDialogConfig.backgroundBitmap.bitmap = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWFRAME});
    engineDialogConfig.backgroundBitmap.rect = engineBayRect;

   // AssetId engineAssetId = state.GetCurrentCar()->GetEngine().assetId;
    
    engineDialogConfig.extraBitmaps = {
        {"button_sidebar", 
            {
                state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WINDOWBUTTONS}),
                {225, 0, 71, 190}
            }
        },
        {"enginetest_v6", 
            {
                state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_V6}),
                {60, 40, 192, 181}
            }
        }
    };
    
    // Set up the callback in the config
    engineDialogConfig.onButtonPressed = buttonCallback;
    
    // Create and return the dialog
    return new Dialog(engineDialogConfig);
}