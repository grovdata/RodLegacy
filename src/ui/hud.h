#pragma once

#include <SDL.h>
#include "core/global.h"
#include "parsers/libp.h"
#include "gfx/bitmap.h"

#include <string>

class HUD: public IDrawable {
   private:
    SrLibColor backgroundColor;
    SrLibColor fontColor;
    std::string lastText;
    
    Bitmap *backgroundBitmap, *textBitmap;
    SDL_Rect statusBarRect, statusBarMiddleRect;
    

   public:
    HUD(StockPalette palette, int backgroundColorIndex, int fontColorIndex);
    ~HUD();
    void Clear();
    void Draw() override;
    void SetVisible(bool visible) override;
    void SetText(const char *text);
    // Redraw with the same text, picking up the current bankroll. The original
    // redraws the bankroll string on every change (AddToBankroll, 0x00222270).
    void Refresh() { SetText(lastText.c_str()); }
    void SetTextOnly(const char *text); // Text without bankroll display
    void AddText(int x, int y, const char *text); // Add additional text at specific position (relative to HUD)
};