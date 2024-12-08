#include "hud.h"
#include "../parsers/libp.h"
#include "../state.h"

const int HUD_YPOS = GAME_HEIGHT - GAME_STATUSBAR_HEIGHT;

HUD::HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex) {
    backgroundBitmap = new Bitmap(GAME_WIDTH, GAME_STATUSBAR_HEIGHT, 32);
    backgroundBitmap->SetVisible(true);
    textBitmap = new Bitmap(GAME_WIDTH, GAME_STATUSBAR_HEIGHT, 32);
    textBitmap->SetVisible(true);
    Palette rgbPal = state.assets.GetStockPalette(palette);
    Color backgroundColor = rgbPal[backgroundColorIndex];
    Color fontColor = rgbPal[fontColorIndex];

    this->backgroundColor = SDL_MapRGB(backgroundBitmap->GetSurface()->format, get<0>(backgroundColor), get<1>(backgroundColor), get<2>(backgroundColor));
    std::cout << "Background color in hex: " << std::hex << this->backgroundColor << std::endl;        
    this->fontColor = SDL_MapRGB(textBitmap->GetSurface()->format, get<0>(fontColor), get<1>(fontColor), get<2>(fontColor));
    std::cout << "Font color in hex: " << std::hex << this->fontColor << std::endl;


    backgroundBitmap->SetPosition(0, HUD_YPOS);
    backgroundBitmap->SetDimensions(GAME_WIDTH, GAME_STATUSBAR_HEIGHT);

    textBitmap->SetPosition(0, HUD_YPOS);
    textBitmap->SetDimensions(GAME_WIDTH, GAME_STATUSBAR_HEIGHT);

    statusBarRect.x = 0;
    statusBarRect.y = HUD_YPOS;
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
    
    // Extract RGB components from fontColor
    uint8_t r = (fontColor >> 16) & 0xFF;
    uint8_t g = (fontColor >> 8) & 0xFF; 
    uint8_t b = fontColor & 0xFF;

    // Set the color modulation before writing text
    SDL_SetSurfaceColorMod(textBitmap->GetSurface(), r, g, b);
    
    // Write the text
    char bankroll[30];
    snprintf(bankroll, 30, "Bankroll: $%d", state.GetBankroll());
    SFont_Write(textBitmap->GetSurface(), state.assets.GetMainFont(), 2, 0, bankroll);
    SFont_Write(textBitmap->GetSurface(), state.assets.GetMainFont(), 100, 0, text);
    
    textBitmap->UpdateFromSurface();
    textBitmap->SaveBitmap(666);
}

void HUD::Clear() {  
    SDL_FillRect(textBitmap->GetSurface(), NULL, 0);
    SDL_FillRect(backgroundBitmap->GetSurface(), &statusBarRect, backgroundColor);
    SDL_FillRect(backgroundBitmap->GetSurface(), &statusBarMiddleRect, backgroundColor);
    backgroundBitmap->UpdateFromSurface();
}