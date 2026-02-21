#include "log.h"
#include "spritesheet.h"

int Spritesheet::GetRows() {
     return rows;
}

int Spritesheet::GetColumns() {
    return columns;
}

void Spritesheet::SelectSprite(int row, int column) {
    if (row < 0 || row >= rows || column < 0 || column >= columns) {
        LOG_WARN("Warning: Invalid sprite coordinates (row=%d, col=%d)\n", row, column);
        return;
    }

    clipRect.w = surface->w / columns;
    clipRect.h = surface->h / rows;
    
    currentY = row;
    currentX = column;
    SetDimensions(clipRect.w, clipRect.h);
    UpdateSprite();

    //LOG_INFO("Spritesheet: selected sprite (%d,%d) rect=[%d,%d,%d,%d]\n", 
    //       row, column, clipRect.x, clipRect.y, clipRect.w, clipRect.h);
}

void Spritesheet::CycleNextSprite() {
    if (currentX == columns - 1) {
        if (currentY == rows - 1) {
            currentX = 0;
            currentY = 0;
        } else {
            currentX = 0;
            currentY++;
        }
    } else {
        currentX++;
    }
    UpdateSprite();
}

void Spritesheet::CyclePrevSprite() {
    if (currentX == 0) {
        if (currentY == 0) {
            currentX = columns - 1;
            currentY = rows - 1;
        } else {
            currentX = columns - 1;
            currentY--;
        }
    } else {
        currentX--;
    }
    UpdateSprite();
}

void Spritesheet::UpdateSprite() {
    clipRect.x = currentX * clipRect.w;
    clipRect.y = currentY * clipRect.h;
}

void Spritesheet::Print() {
    LOG_INFO("SelectSprite %d,%d: [%d,%d, %d, %d]", currentX, currentY, clipRect.x, clipRect.y, clipRect.w, clipRect.h);
}

void Spritesheet::Draw() {
    if (!isVisible) return;
    if (drawFullSheet)
        SDL_RenderCopy(renderer, texture.get(), nullptr, &rect);
    else
        SDL_RenderCopy(renderer, texture.get(), &clipRect, &rect);
}
