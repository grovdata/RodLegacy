#include "state.h"
#include "engine_bay.h"

Dialog* EngineBay::CreateDialog(State& state) {

    // sidebar index 295, 324 when tuning

 
    const int buttonWidth = 61;
    const int buttonHeight = 31;


    SDL_Rect engineBayRect = {24, 0, 224, 190};
    
    SDL_Rect tuneRect = {231, 20, buttonWidth, buttonHeight};
    SDL_Rect partsRect = {231, 79, buttonWidth, buttonHeight};
    SDL_Rect doneRect = {231, 139, buttonWidth, buttonHeight};
    
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


    vector<SDL_Rect> lines = {
        {85, 89, 150, 1},
    };

    
    DialogConfig engineDialogConfig;
    engineDialogConfig.bounds = engineBayRect;
    engineDialogConfig.backgroundColor = {0, 208, 0, 255};
    engineDialogConfig.borderColor = {0, 111, 239, 255};
    engineDialogConfig.buttons = buttons;
    engineDialogConfig.lines = lines;
    engineDialogConfig.zones = dialogZones;
    engineDialogConfig.contentAreas = vector<SDL_Rect>{engineBayRect};
    engineDialogConfig.font = state.assets.GetMainFont();
    
    // Set up the callbacks in the config
    engineDialogConfig.onButtonPressed = [](int buttonIndex) {
        switch (buttonIndex) {
            case 0:
                SDL_Log("Garage::engineDialogConfig::onButtonPressed: Tune");
                // Handle tune action
                break;
            case 1:
                SDL_Log("Garage::engineDialogConfig::onButtonPressed: Parts");
                // Handle parts action
                break;
            case 2:
                SDL_Log("Garage::engineDialogConfig::onButtonPressed: Done");
                // Handle done action
                break;
        }
    };
    
    // Create and return the dialog
    return new Dialog(engineDialogConfig);
}