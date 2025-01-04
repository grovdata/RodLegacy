#pragma once

#include "global.h"
#include "bitmap.h"

class Spritesheet : public Bitmap {
private:
    int rows;
    int columns;
    int currentRow;
    int currentColumn;
    int currentX;
    int currentY;

    SDL_Rect clipRect;

public:
    // Default constructor
    Spritesheet() : Bitmap(), rows(1), columns(1), currentRow(0), currentColumn(0), currentX(0), currentY(0), clipRect({0, 0, 0, 0}) {}
    
    // Constructor with dimensions
    Spritesheet(int rows, int columns) 
        : Bitmap(), rows(rows), columns(columns), currentRow(0), currentColumn(0), currentX(0), currentY(0), clipRect({0, 0, 0, 0}) {}
    
    // Copy constructor
    Spritesheet(const Spritesheet& other)
        : Bitmap(other), rows(other.rows), columns(other.columns),
          currentRow(other.currentRow), currentColumn(other.currentColumn), currentX(other.currentX), currentY(other.currentY), clipRect(other.clipRect) {}
    
    virtual ~Spritesheet() {
        Bitmap::~Bitmap();
        SDL_Log("Deleting Spritesheet virtual");
    }
    void SelectSprite(int row, int column);
    void UpdateSprite();
    void CycleNextSprite();
    void Draw() override;
    void Print();
    int GetRows();
    int GetColumns();
};