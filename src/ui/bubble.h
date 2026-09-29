/*
 * RodLegacy - Speech bubbles: Bubble shows a line of text in one of the original's three message boxes.
 *
 * Created: 2023-02-12
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "gfx/bitmap.h"
#include "core/utils.h"
#include "ui/zone.h"

typedef enum {
    BUBBLE_LEFT,            // box 1: wide, tail down-left,  at (80,30)
    BUBBLE_RIGHT,           // box 0: narrow, tail down-right, at (72,30)
    BUBBLE_RIGHT_TWO_LINES  // box 2: wide, two rows,         at (80,20)
} BubbleStyle;

class Bubble: public IDrawable {
    private:
        uint64_t timerStart;
        BubbleStyle style;
        sdl2::SurfacePtr fontSurface;
        sdl2::TexturePtr fontTexture;
        SDL_Rect fontRect;
        Bitmap *bubble;

        void ApplyStyle(BubbleStyle style);
        void UpdateText(std::string& text);

    public:
        Bubble(std::string text, BubbleStyle style, bool autoDismiss);
        ~Bubble();
        virtual void Draw() override;
        virtual void SetVisible(bool visible) override;
        void SetText(std::string text) { UpdateText(text); }
        void SetText(std::string text, BubbleStyle style) { ApplyStyle(style); UpdateText(text); }
};
