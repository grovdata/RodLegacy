#pragma once

#include <SDL.h>
#include "../global.h"
#include "../parsers/libp.h"
#include "../bitmap.h"

using namespace std;

class HUD: public IDrawable {
   private:
    SrLibColor backgroundColor;
    SrLibColor fontColor;
    
    Bitmap *backgroundBitmap, *textBitmap;
    SDL_Rect statusBarRect, statusBarMiddleRect;
    

   public:
    HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex);
    ~HUD();
    void Clear();
    void Draw() override;
    void SetVisible(bool visible) override;
    void SetText(const char *text);
};