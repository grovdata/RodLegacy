#pragma once

#include "global.h"

class Spritesheet {
   public:
    Spritesheet(char const *path, int rows, int columns);
    ~Spritesheet();

    void SelectSprite(int x, int y);
    void UpdateSprite();
    void CycleNextSprite();
    void DrawSprite(SDL_Rect *position);

   private:
    SDL_Rect clipRect;
    SDL_Texture *texture;
    SDL_Surface *surface;

    int m_start_x;
    int m_start_y;

    int rows;
    int columns;
    int currentX;
    int currentY;
};