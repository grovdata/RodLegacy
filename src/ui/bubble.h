#pragma once

#include "../global.h"
#include "../bitmap.h"
#include "zone.h"

// TODO: More flexible speech bubbles with 9-patch?

using namespace std;

typedef enum {
    BUBBLE_LEFT,
    BUBBLE_RIGHT,
    BUBBLE_RIGHT_TWO_LINES
} BubbleStyle;

class Bubble: public Bitmap {
    private:
        SDL_Surface *fontSurface;
        SDL_Texture *fontTexture;
        SDL_Rect fontRect;

        void UpdateText(string text, int xPos, int yPos);

        SDL_TimerID timerID;
        static Uint32 AnimationTimerCallback(Uint32 interval, void* param);

    public:
        Bubble(string text, BubbleStyle style, int xPos, int yPos, bool autoDismiss);
        ~Bubble();

        SDL_Texture *GetTexture();
        SDL_Rect* GetRect();

        void Draw();
};
