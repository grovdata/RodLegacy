#pragma once

#include "global.h"
#include "bitmap.h"

class Spritesheet: public Bitmap {
   public:
    Spritesheet(int rows, int columns);
    Spritesheet(int rows, int columns, Spritesheet *other);
    virtual ~Spritesheet() {
        SDL_Log("Deleting Spritesheet virtual");
    }
    void SelectSprite(int row, int column);
    void UpdateSprite();
    void CycleNextSprite();
    void Draw() override;
    void Print();
    int GetRows();
    int GetColumns();

   private:
    SDL_Rect clipRect;

    int rows;
    int columns;
    int currentX;
    int currentY;
};