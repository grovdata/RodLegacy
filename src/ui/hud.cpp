#include "hud.h"
#include "../parsers/libp.h"
#include "../state.h"

const int HUD_YPOS = GAME_HEIGHT - GAME_STATUSBAR_HEIGHT;

HUD::HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex) {
    backgroundBitmap = new Bitmap(GAME_WIDTH, GAME_STATUSBAR_HEIGHT, 32);
    backgroundBitmap->SetVisible(true);
    textBitmap = new Bitmap(GAME_WIDTH, GAME_STATUSBAR_HEIGHT, 32);
    textBitmap->SetVisible(true);
    Palette rgbPal = state.GetAssets().GetStockPalette(palette);
    SrLibColor backgroundColor = rgbPal[backgroundColorIndex];
    SrLibColor fontColor = rgbPal[fontColorIndex];
    
    this->backgroundColor = backgroundColor;
    this->fontColor = fontColor;

    backgroundBitmap->SetPosition(0, HUD_YPOS);
    backgroundBitmap->SetDimensions(GAME_WIDTH, GAME_STATUSBAR_HEIGHT);

    textBitmap->SetPosition(0, HUD_YPOS+1);
    textBitmap->SetDimensions(GAME_WIDTH, GAME_STATUSBAR_HEIGHT);

    statusBarRect.x = 0;
    statusBarRect.y = 0;
    statusBarRect.w = GAME_WIDTH;
    statusBarRect.h = GAME_STATUSBAR_HEIGHT;

    statusBarMiddleRect = statusBarRect;
    statusBarMiddleRect.x = GAME_WIDTH/2;
}

HUD::~HUD() {
    delete textBitmap;
}   

void HUD::SetVisible(bool visible) {
    this->visible = visible;
    textBitmap->SetVisible(visible);
}

void HUD::Draw() {
    if (visible) {
        backgroundBitmap->Draw();
        textBitmap->Draw();
    }
}

void HUD::SetText(const char *text) {
    Clear();
    
    // Use the stored RGB components directly
    SDL_SetSurfaceColorMod(textBitmap->GetSurface(), 
        fontColor.r, 
        fontColor.g, 
        fontColor.b);
    
    // Write the text
    char bankroll[30];
    snprintf(bankroll, 30, "Bankroll: $%d", state.GetBankroll());
    SFont_Write(textBitmap->GetSurface(), state.GetAssets().GetMainFont(), 2, 0, bankroll);
    SFont_Write(textBitmap->GetSurface(), state.GetAssets().GetMainFont(), 100, 0, text);
    
    textBitmap->UpdateFromSurface();    
}

void HUD::SetTextOnly(const char *text) {
    Clear();
    
    // Use the stored RGB components directly
    SDL_SetSurfaceColorMod(textBitmap->GetSurface(), 
        fontColor.r, 
        fontColor.g, 
        fontColor.b);
    
    // Write the text without bankroll
    SFont_Write(textBitmap->GetSurface(), state.GetAssets().GetMainFont(), 2, 0, text);
    
    textBitmap->UpdateFromSurface();    
}

void HUD::AddText(int x, int y, const char *text) {
    // Add additional text at a specific position (doesn't clear)
    // Use the stored RGB components directly
    SDL_SetSurfaceColorMod(textBitmap->GetSurface(), 
        fontColor.r, 
        fontColor.g, 
        fontColor.b);
    
    // Write the text at the specified position
    SFont_Write(textBitmap->GetSurface(), state.GetAssets().GetMainFont(), x, y, text);
    
    textBitmap->UpdateFromSurface();    
}

void HUD::Clear() {
    Uint32 sdlColor = SDL_MapRGB(backgroundBitmap->GetSurface()->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);
    SDL_FillRect(textBitmap->GetSurface(), NULL, 0);
    SDL_FillRect(backgroundBitmap->GetSurface(), &statusBarRect, sdlColor);
    SDL_FillRect(backgroundBitmap->GetSurface(), &statusBarMiddleRect, sdlColor);
    backgroundBitmap->UpdateFromSurface();
}