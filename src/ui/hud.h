#pragma once

#include <SDL.h>
#include "../global.h"
#include "../parsers/libp.h"
#include "../bitmap.h"

using namespace std;

class HUD {
   private:
    uint16_t backgroundColor;
    uint16_t fontColor;
    Bitmap *textBitmap;
    SDL_Rect statusBarRect, statusBarMiddleRect;

   public:
    HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex);
    ~HUD();
    void Clear();
    void Draw();
    void SetText(const char *text);
};