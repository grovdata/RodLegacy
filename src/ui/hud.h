/*
 * RodLegacy - The bottom status bar: HUD with the bankroll and hover text.
 *
 * Created: 2024-12-08
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

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
    HUD(const Palette& rgbPalette, int backgroundColorIndex, int fontColorIndex);
    ~HUD();
    void Clear();
    void Draw() override;
    void SetVisible(bool visible) override;
    void SetText(const char *text);
    void Refresh() { SetText(lastText.c_str()); }
    void SetTextOnly(const char *text);
    void AddText(int x, int y, const char *text);
};