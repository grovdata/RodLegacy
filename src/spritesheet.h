#pragma once

#include "global.h"
#include "bitmap.h"

class Spritesheet: public Bitmap {
   public:
    Spritesheet(int rows, int columns);
    void SelectSprite(int x, int y);
    void UpdateSprite();
    void CycleNextSprite();
    void Draw() override;

   private:
    SDL_Rect clipRect;

    int rows;
    int columns;
    int currentX;
    int currentY;
};