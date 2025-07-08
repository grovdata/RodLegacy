#include "newspaper.h"
#include "state.h"

const std::string autoParts = "------------ PARTS ------------";
const std::string usedCars = "---------- USED CARS ----------";

Newspaper::Newspaper() : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU}) {
    page = 0;
}

Newspaper::~Newspaper() {

}

void Newspaper::Init(bool debugMode) {
    Scene::Init(debugMode);
    InitPageOne();
    InitPageAds();

    Update();
    // Common

    SDL_Rect autoPartsRect = {0, 0, GAME_WIDTH, 32};
    Zone autoPartsZone(ZONE_MAGAZINE_AUTO_PARTS, strdup("Auto Parts"), &autoPartsRect);

    SDL_Rect usedCarsRect = {0, 33, GAME_WIDTH, 33};
    Zone usedCarsZone(ZONE_MAGAZINE_USED_CARS, strdup("Used Cars"), &usedCarsRect);

    SDL_Rect backToGarageRect = {0, 66, GAME_WIDTH, 125};
    Zone backToGarageZone(ZONE_MAGAZINE_BACK_TO_GARAGE, strdup("Go to the garage"), &backToGarageRect);

    SDL_Rect previousRect = {0, 0, 64, 190};
    Zone previousZone(ZONE_MAGAZINE_PREVIOUS, strdup("Previous page"), &previousRect);

    SDL_Rect nextRect = {GAME_WIDTH - 64, 0, 64, 190};
    Zone nextZone(ZONE_MAGAZINE_NEXT, strdup("Next page"), &nextRect);

    SDL_Rect backToGarage2Rect = {0, 199, GAME_WIDTH, 125};
    Zone backToGarage2Zone(ZONE_MAGAZINE_BACK_TO_GARAGE2, strdup("Go to the garage"), &backToGarage2Rect);

    zones.push_back(autoPartsZone);
    zones.push_back(usedCarsZone);
    zones.push_back(backToGarageZone);
    zones.push_back(previousZone);
    zones.push_back(nextZone);
    zones.push_back(backToGarage2Zone);
}

void Newspaper::InitPageOne() {
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
}

void Newspaper::InitPageAds() {
    // Ads page 1-x
    SDL_Rect textAreaRect = {64, 0, 192, 190};

    adsArea = Bitmap(textAreaRect.w, textAreaRect.h, 32);
    SDL_FillRect(adsArea.GetSurface(), NULL, SDL_MapRGBA(adsArea.GetSurface()->format, 240, 240, 240, 255));
    adsArea.UpdateFromSurface();
    adsArea.SetPosition(textAreaRect.x, textAreaRect.y);
    adsArea.SetVisible(false);

    AddDivider(1);

    autoPartsHeader.Init(state.assets.GetMainFont(), 68, 12, 0);
    autoPartsHeader.SetText(autoParts, false, 184);

    adsArea.UpdateFromSurface();

    test.Init(state.assets.GetMainFont(), 68, 32, 0);
    test.SetText("1940 Chevrolet 2-dr coupe. Auto.\nRuns good. $400", false, 184);

    AddDivider(31);
}

void Newspaper::AddDivider(int y) {
    SDL_Rect dividerRect = {4, y, 184, 1};
    SDL_FillRect(adsArea.GetSurface(), &dividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
    adsArea.UpdateFromSurface();
}

void Newspaper::Draw() {
    Scene::Draw();
    exit.Draw();
    
    headline.Draw();
    date.Draw();
    headingLeft.Draw();
    headingRight.Draw();

    previous.Draw();
    adsArea.Draw();
    next.Draw();

    autoPartsHeader.Draw();
    usedCarsHeader.Draw();
    test.Draw();
}

void Newspaper::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
}

void Newspaper::Action(ZoneId zone) {
    Scene::Action(zone);
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    switch (zone) { 
        case ZONE_MAGAZINE_AUTO_PARTS:
            page = 1;
            break;
        case ZONE_MAGAZINE_USED_CARS:
            page = 4;
            break;
        case ZONE_MAGAZINE_BACK_TO_GARAGE:
            state.MoveToScene(SCENE_GARAGE);
            break;
        case ZONE_MAGAZINE_PREVIOUS:
            if (page > 0) {
                page--;
            } else {
                page = 6;
            }
            break;
        case ZONE_MAGAZINE_NEXT:
            if (page < 6) {
                page++;
            } else {
                page = 0;
            }
            break;
    }

    SDL_Log("Page: %d", page);

    Update();
}

void Newspaper::Update() {
    background->SetVisible(page == 0);
    headline.SetVisible(page == 0);
    headingLeft.SetVisible(page == 0);
    headingRight.SetVisible(page == 0);
    previous.SetVisible(page > 0);
    next.SetVisible(page > 0);
    adsArea.SetVisible(page > 0);
    
    autoPartsHeader.SetVisible(page == 1);
    test.SetVisible(page == 1);
    
    usedCarsHeader.SetVisible(page == 4);

    SetZoneEnabled(ZONE_MAGAZINE_AUTO_PARTS, page == 0);
    SetZoneEnabled(ZONE_MAGAZINE_USED_CARS, page == 0);
    SetZoneEnabled(ZONE_MAGAZINE_BACK_TO_GARAGE, page == 0);
    SetZoneEnabled(ZONE_MAGAZINE_PREVIOUS, page > 0);
    SetZoneEnabled(ZONE_MAGAZINE_NEXT, page > 0);
}
