#include "spritesheet.h"

Spritesheet::Spritesheet(int rows, int columns) : Bitmap() {
    this->rows = rows;
    this->columns = columns;


    // printf("Spritesheet surface width: %d height:%d %d \n", surface->w, surface->h,columns);
    // printf("Spritesheet cell width: %d height:%d \n", clipRect.w, clipRect.h);
}

void Spritesheet::SelectSprite(int x, int y) {
    if (surface)

    clipRect.w = surface->w / this->columns;
    clipRect.h = surface->h / this->rows;

    currentX = x;
    currentY = y;
    UpdateSprite();
    //printf("rect %d %d %d %d\n", m_clip.x, m_clip.y, m_clip.w, m_clip.y);
    //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, rect.w, rect.h);
    //SDL_UpdateTexture(texture, NULL, surface->pixels + m_clip.x + m_clip.y * surface->pitch, surface->pitch);
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

void Spritesheet::UpdateSprite() {
    clipRect.x = currentX * clipRect.w;
    clipRect.y = currentY * clipRect.h;
}


void Spritesheet::Draw() {
    if (isVisible)
        SDL_RenderCopy(renderer, texture, &clipRect, &rect);
}
