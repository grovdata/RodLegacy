#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "../global.hpp"

const int cButtonStandardHeight = 19;
const int cButtonStandardTextOffsetY = 5;


class Button {
    private:
        SDL_Surface *m_Surface;
        SDL_Texture *m_Texture;
        SDL_Rect m_Rect;
    public:
        Button(char* text, int offsetX, int offsetY, int width, int height);
        ~Button();

        SDL_Texture *GetTexture();
        SDL_Rect* GetRect();

};

#endif