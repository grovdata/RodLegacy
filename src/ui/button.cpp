#include "button.hpp"

Button::Button(char* text, int offsetX, int offsetY, int width, int height) {
    m_Surface = SDL_CreateRGBSurface(0, width, height, 32,
                                        0x00FF0000,
                                        0x0000FF00,
                                        0x000000FF,
                                        0xFF000000);

    m_Rect.x = 0; m_Rect.y = 0; m_Rect.w = width; m_Rect.h = height;
    SDL_FillRect(m_Surface, &m_Rect, SDL_MapRGB(m_Surface->format, 0, 100, 0));

    SDL_Rect border = { 1, 1, width - 2, height - 2};
    SDL_FillRect(m_Surface, &border, SDL_MapRGB(m_Surface->format, 255, 255, 255));

    SDL_Rect innerFill = { 2, 2, width - 4, height - 4};
    SDL_FillRect(m_Surface, &innerFill, SDL_MapRGB(m_Surface->format, 0, 100, 0));

    SFont_Write(m_Surface, font, 3, cButtonStandardTextOffsetY, text);
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