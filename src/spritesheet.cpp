#include "spritesheet.h"

Spritesheet::Spritesheet(int rows, int columns) : Bitmap() {
    this->rows = rows;
    this->columns = columns;
}

Spritesheet::Spritesheet(int rows, int columns, Spritesheet *other) {
    this->rows = other->GetRows();
    this->columns = other->GetColumns();
    this->LoadSurface(other->surface);
}

int Spritesheet::GetRows() {
     return rows;
}

int Spritesheet::GetColumns() {
    return columns;
}

void Spritesheet::SelectSprite(int row, int column) {
    clipRect.w = surface->w / this->columns;
    clipRect.h = surface->h / this->rows;
    currentY = row;
    currentX = column;
    SetDimensions(clipRect.w, clipRect.h);
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

void Spritesheet::Print() {
    cout << "SelectSprite " << currentX << "," << currentY << ": [" << clipRect.x << "," << clipRect.y << ", " << clipRect.w << ", " << clipRect.h << "]" << endl;
}

void Spritesheet::Draw() {
    if (isVisible)
        SDL_RenderCopy(renderer, texture, &clipRect, &rect);
}
