#ifndef MOUSE_H
#define MOUSE_H

#include "global.hpp"

// TODO: Enum for all cursor types
// Hand
// Wrench
// Fuelpump
// Rag

class Mouse {
    private:
        SDL_Texture *cursorTexture;
        SDL_Rect rect;
        SDL_Rect logicalRect;
    public:

        Mouse() {
            cursorTexture = IMG_LoadTexture(renderer, "../data/pointer1.png");
            rect.w = 16;
            rect.h = 12;
            logicalRect.w = 16;
            logicalRect.h = 12;
            SDL_ShowCursor(false);
        }

        ~Mouse() {
            SDL_DestroyTexture(cursorTexture);
        }

        void update() {
            SDL_GetMouseState(&rect.x, &rect.y);

            // We must convert coordinates into logical ones since gfx is scaled
            float logicalX, logicalY;
            SDL_RenderWindowToLogical(renderer, rect.x, rect.y, &logicalX, &logicalY);
            logicalRect.x = (int)logicalX;
            logicalRect.y = (int)logicalY;
        }

        void draw() {
            SDL_RenderCopy(renderer, cursorTexture, NULL, &logicalRect);
        }
};

#endif