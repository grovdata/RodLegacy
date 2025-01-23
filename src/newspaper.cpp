#include "newspaper.h"
#include "state.h"

Newspaper::Newspaper() : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU}) {
   
}

Newspaper::~Newspaper() {

}

void Newspaper::Init(bool debugMode) {
    Scene::Init(debugMode);
    exit = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    exit.SetPosition(100, 100);
    exit.SetVisible(false);

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
    date.Init(state.assets.GetMainFont(), 96, 120, dateColor);
    date.SetText("  June 10, 1963  ", true);

    headingLeft.Init(state.assets.GetNewspaperFont(), 10, 138, dateColor);
    headingLeft.SetText("  LBJ insists: ", true);

    headingRight.Init(state.assets.GetNewspaperFont(), 10, 138 + 15, dateColor);
    headingRight.SetText("  \"No U.S. soldiers in Viet Nam.\" ", true);
}

void Newspaper::Draw() {
    Scene::Draw();
    exit.Draw();
    
    headline.Draw();
    date.Draw();
    headingLeft.Draw();
    headingRight.Draw();
}