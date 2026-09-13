#pragma once

#include "core/global.h"
#include "gfx/bitmap.h"
#include "core/utils.h"
#include "ui/zone.h"

// TODO: More flexible speech bubbles with 9-patch?

typedef enum {
    BUBBLE_LEFT,
    BUBBLE_RIGHT,
    BUBBLE_RIGHT_TWO_LINES
} BubbleStyle;



class Bubble: public IDrawable {
    private:
        uint64_t timerStart;
        sdl2::SurfacePtr fontSurface;
        sdl2::TexturePtr fontTexture;
        SDL_Rect fontRect;
        Bitmap *bubble;

        void UpdateText(std::string& text);


    public:
        Bubble(std::string text, BubbleStyle style, int xPos, int yPos, bool autoDismiss);
        ~Bubble();
        virtual void Draw() override;
        virtual void SetVisible(bool visible) override;
        void SetText(std::string text) { UpdateText(text); }
};
