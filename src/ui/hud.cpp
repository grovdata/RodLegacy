#include "hud.h"
#include "../state.h"

HUD::HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex) {
    textBitmap = new Bitmap(GAME_WIDTH, GAME_HEIGHT, 32);
    textBitmap->SetVisible(true);
    vector<uint16_t> paletteData = BitmapParser::GetStockPalette(palette);

    Color color = BitmapParser::GetRGBFromRGB4(paletteData[backgroundColorIndex]);
    this->backgroundColor = SDL_MapRGB(textBitmap->GetSurface()->format, get<0>(color), get<1>(color), get<2>(color));
    Color fontColor = BitmapParser::GetRGBFromRGB4(paletteData[fontColorIndex]);
    this->fontColor = SDL_MapRGB(textBitmap->GetSurface()->format, get<0>(fontColor), get<1>(fontColor), get<2>(fontColor));

    statusBarRect.x = 0;
    statusBarRect.y = GAME_HEIGHT - GAME_STATUSBAR_HEIGHT;
    statusBarRect.w = GAME_WIDTH;
    statusBarRect.h = GAME_STATUSBAR_HEIGHT;

    statusBarMiddleRect = statusBarRect;
    statusBarMiddleRect.x = GAME_WIDTH/2;
}

HUD::~HUD() {
    delete textBitmap;
}   

void HUD::Draw() {
    textBitmap->Draw();
    textBitmap->UpdateFromSurface();
}

void HUD::SetText(const char *text) {
    char bankroll[30];
    snprintf(bankroll, 30, "Bankroll: $%d", state.GetBankroll());
    SFont_Write(textBitmap->GetSurface(), state.assets.GetMainFont(), 2, GAME_HEIGHT - GAME_STATUSBAR_HEIGHT, bankroll);
    SFont_Write(textBitmap->GetSurface(), state.assets.GetMainFont(), 160, GAME_HEIGHT - GAME_STATUSBAR_HEIGHT, text);
        
    uint8_t r = (fontColor >> 11) & 0x1F;
    uint8_t g = (fontColor >> 5) & 0x3F;
    uint8_t b = (fontColor >> 0) & 0x1F;
    SDL_SetTextureColorMod(textBitmap->GetTexture(), r, g, b);
}

void HUD::Clear() {   
    SDL_FillRect(textBitmap->GetSurface(), &statusBarMiddleRect, backgroundColor);
}