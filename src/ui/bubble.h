#pragma once

#include "../global.h"
#include "../bitmap.h"
#include "zone.h"

// TODO: More flexible speech bubbles with 9-patch?

typedef enum {
    BUBBLE_LEFT,
    BUBBLE_RIGHT,
    BUBBLE_RIGHT_TWO_LINES
} BubbleStyle;

class Bubble: public Bitmap {
    public:
        Bubble(std::string text, BubbleStyle style);
        ~Bubble();

        SDL_Texture *GetTexture();
        SDL_Rect* GetRect();

        void Draw();
};
