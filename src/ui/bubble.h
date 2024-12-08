#pragma once

#include "../global.h"
#include "../bitmap.h"
#include "../utils.h"
#include "zone.h"

// TODO: More flexible speech bubbles with 9-patch?

using namespace std;

typedef enum {
    BUBBLE_LEFT,
    BUBBLE_RIGHT,
    BUBBLE_RIGHT_TWO_LINES
} BubbleStyle;

class IDrawable {
    protected:
        bool visible;
    public:
        virtual ~IDrawable() = default;
        virtual void Draw() = 0;
        virtual void SetVisible(bool visible) = 0;
};

class Bubble: public IDrawable {
    private:
        uint64_t timerStart;
        sdl2::SurfacePtr fontSurface;
        sdl2::TexturePtr fontTexture;
        SDL_Rect fontRect;
        Bitmap *bubble;

        void UpdateText(string text, int xPos, int yPos);


    public:
        Bubble(string text, BubbleStyle style, int xPos, int yPos, bool autoDismiss);
        ~Bubble();
        virtual void Draw() override;
        virtual void SetVisible(bool visible) override;
};
