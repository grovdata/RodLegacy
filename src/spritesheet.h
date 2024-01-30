#pragma once

#include "global.h"

class Spritesheet {
   public:
    Spritesheet(char const *path, int row, int column);
    ~Spritesheet();

    void SelectSprite(int x, int y);
    void DrawSprite(SDL_Rect *position);

   private:
    SDL_Rect m_clip;
    SDL_Texture *texture;
    SDL_Surface *surface;

    int m_start_x;
    int m_start_y;
};