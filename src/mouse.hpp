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
        SDL_Rect point;
    public:

        Mouse() {
            cursorTexture = IMG_LoadTexture(renderer, "../data/pointer1.png");
            rect.w = 16;
            rect.h = 12;
            point.w = 1;
            point.h = 1;
            SDL_ShowCursor(false);
        }

        ~Mouse() {
            SDL_DestroyTexture(cursorTexture);
        }

        void update() {
            SDL_GetMouseState(&rect.x, &rect.y);
            point.x = rect.x;
            point.y = rect.y;

            cout << "mouse" << point.x << endl;
        }

        void draw() {
            SDL_RenderCopy(renderer, cursorTexture, NULL, &rect);
        }
};

#endif