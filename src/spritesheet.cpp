#include "spritesheet.h"

Spritesheet::Spritesheet(char const *path, int rows, int columns) {
    this->rows = rows;
    this->columns = columns;

    surface = IMG_Load(path);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    printf("Spritesheet surface width: %d height:%d %d \n", surface->w, surface->h,columns);
    clipRect.w = surface->w / columns;
    clipRect.h = surface->h / rows;
    printf("Spritesheet cell width: %d height:%d \n", clipRect.w, clipRect.h);
   //  m_clip.w = 192;
    // m_clip.h = 124;

    // TODO Don't hardcode sheet
    //m_clip = {0,0, 38, 60};
}

Spritesheet::~Spritesheet() {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void Spritesheet::SelectSprite(int x, int y) {
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

void Spritesheet::DrawSprite(SDL_Rect *position) {
    SDL_RenderCopy(renderer, texture, &clipRect, position);
}