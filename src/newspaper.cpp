#include "newspaper.h"
#include "state.h"

Newspaper::Newspaper() : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU}) {
   
}

Newspaper::~Newspaper() {

}

void Newspaper::Init(bool debugMode) {
    Scene::Init(debugMode);
    previous = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    previous.SetPosition(0, 0);
    SDL_Rect previousRect = {0, 0, 64, 190};
    previous.Crop(previousRect);
    previous.SetDimensions(64, 190);
    previous.UpdateFromSurface();
    previous.SetVisible(false);

    next = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    next.SetPosition(GAME_WIDTH - 64, 0);
    SDL_Rect nextRect = {64, 0, 64, 190};
    next.Crop(nextRect);
    next.SetDimensions(64, 190);
    next.UpdateFromSurface();
    next.SetVisible(false);

    // Set headline based on date?
    AmigaLibAsset headlineAsset = NEWSPAPER_NO_SOLDIERS_IN_VIETNAM;
    headline = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, headlineAsset});
    headline.SetPosition(0, 136);
    headline.SetVisible(true); 

    const SDL_Rect masks[5][2] = {
        {{0, 0, 144, 16}, {146, 0, 175, 30}},
        {{0, 0, 144, 30}, {146, 0, 175, 30}},
        {{0, 0, 144, 30}, {146, 0, 175, 30}},
        {{0, 0, 145, 24}, {146, 0, 175, 24}},
        {{0, 0, 145, 32}, {146, 0, 175, 32}}
    };

    // Temp
    SDL_Rect headlineMaskRect[2];
    headlineMaskRect[0] = masks[headlineAsset - NEWSPAPER_DODGERS_MALIBU][0];
    headlineMaskRect[1] = masks[headlineAsset - NEWSPAPER_DODGERS_MALIBU][1];
    // SDL_Rect headlineMaskRect = {0, 0, 175, 30};
    SDL_FillRect(headline.GetSurface(), &headlineMaskRect[0], SDL_MapRGBA(headline.GetSurface()->format, 240, 240, 240, 255));
    SDL_FillRect(headline.GetSurface(), &headlineMaskRect[1], SDL_MapRGBA(headline.GetSurface()->format, 240, 240, 240, 255));
    headline.UpdateFromSurface();
    
    uint32_t dateColor = SDL_MapRGB(surface->format, 32, 32, 32);
    date.Init(state.assets.GetMainFont(), 0, 120, dateColor);
    date.SetText("  June 10, 1963  ", true, GAME_WIDTH);

    headingLeft.Init(state.assets.GetNewspaperFont(), 10, 138, dateColor);
    headingLeft.SetText("  President insists: ", true, GAME_WIDTH);

    headingRight.Init(state.assets.GetNewspaperFont(), 10, 138 + 15, dateColor);
    headingRight.SetText("  \"No U.S. soldiers in Viet Nam\". ", true, GAME_WIDTH);

    SDL_Rect autoPartsRect = {0, 0, GAME_WIDTH, 32};
    Zone autoPartsZone(ZONE_MAGAZINE_AUTO_PARTS, strdup("Auto Parts"), &autoPartsRect);

    SDL_Rect usedCarsRect = {0, 33, GAME_WIDTH, 33};
    Zone usedCarsZone(ZONE_MAGAZINE_USED_CARS, strdup("Used Cars"), &usedCarsRect);

    SDL_Rect backToGarageRect = {0, 66, GAME_WIDTH, 125};
    Zone backToGarageZone(ZONE_MAGAZINE_BACK_TO_GARAGE, strdup("Go to the garage"), &backToGarageRect);

    zones.push_back(autoPartsZone);
    zones.push_back(usedCarsZone);
    zones.push_back(backToGarageZone);
}

void Newspaper::Draw() {
    Scene::Draw();
    exit.Draw();
    
    headline.Draw();
    date.Draw();
    headingLeft.Draw();
    headingRight.Draw();
    previous.Draw();
    next.Draw();
}

void Newspaper::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
}

void Newspaper::Action(ZoneId zone) {
    Scene::Action(zone);
    if (sceneTransition != SCENE_SOLID) {
        return;
    }
    if (zone == ZONE_MAGAZINE_AUTO_PARTS) {
        page = 1;
    }
    if (zone == ZONE_MAGAZINE_USED_CARS) {
        state.MoveToScene(SCENE_GAS_STATION);
    }
    if (zone == ZONE_MAGAZINE_BACK_TO_GARAGE) {
        state.MoveToScene(SCENE_GARAGE);
    } 

    background->SetVisible(page == 0);
    headline.SetVisible(page == 0);
    previous.SetVisible(page > 0);
    next.SetVisible(page > 0);
}