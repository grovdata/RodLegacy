#pragma once

#include "../global.h"
#include "zone.h"

const int cButtonStandardHeight = 19;
const int cButtonStandardTextOffsetY = 5;
// testButton = new Button("Pink slips", 100, 100, 82, cButtonStandardHeight);
    // testButton2 = new Button("Drag race", 100, 100+cButtonStandardHeight, 82, cButtonStandardHeight);

class Button {
    private:
        SDL_Surface *m_Surface;
        SDL_Texture *m_Texture;
        SDL_Rect m_Rect;
    public:
        Button(std::string text, int offsetX, int offsetY, int width, int height);
        ~Button();

        SDL_Texture *GetTexture();
        SDL_Rect* GetRect();

};
