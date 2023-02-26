#include "button.h"

#define COL_GREEN 0, 100, 110
#define COL_WHITE 255, 255, 255

Button::Button(std::string text, int offsetX, int offsetY, int width, int height) {
    //SDL_Rect rect = { offsetX, offsetY, width, height };
    //Zone::Zone(text, &rect);
    m_Surface = SDL_CreateRGBSurface(0, width, height, 32,
                                        0x00FF0000,
                                        0x0000FF00,
                                        0x000000FF,
                                        0xFF000000);

    m_Rect.x = offsetX; m_Rect.y = offsetY; m_Rect.w = width; m_Rect.h = height;
    SDL_FillRect(m_Surface, &m_Rect, SDL_MapRGB(m_Surface->format, COL_GREEN));

    SDL_Rect border = { 1, 1, width , height - 2};
    SDL_FillRect(m_Surface, &border, SDL_MapRGB(m_Surface->format, COL_WHITE));

    SDL_Rect innerFill = { 2, 2, width - 3, height - 4};
    SDL_FillRect(m_Surface, &innerFill, SDL_MapRGB(m_Surface->format, COL_GREEN));

    SFont_Write(m_Surface, font, 3, cButtonStandardTextOffsetY, text.c_str());
    m_Texture = SDL_CreateTextureFromSurface(renderer, m_Surface);
}

Button::~Button() {

}

SDL_Texture *Button::GetTexture() {
    return m_Texture;
}

SDL_Rect *Button::GetRect() {
    return &m_Rect;
}