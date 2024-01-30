#include "spritesheet.h"

Spritesheet::Spritesheet(char const *path, int row, int column) {
    surface = IMG_Load(path);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    //m_clip.w = surface->w / column;
    //m_clip.h = surface->h / row;
    // m_clip.w = 192;
    // m_clip.h = 124;

    m_clip = {0,0, 38, 60};
}

Spritesheet::~Spritesheet() {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void Spritesheet::SelectSprite(int x, int y) {
    m_clip.x = x * m_clip.w;
    m_clip.y = y * m_clip.h;
    //printf("rect %d %d %d %d %d %d\n",surface->w ,surface->h, m_clip.x, m_clip.y, m_clip.w, m_clip.y);
    //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, rect.w, rect.h);
    //SDL_UpdateTexture(texture, NULL, surface->pixels + m_clip.x + m_clip.y * surface->pitch, surface->pitch);
}

void Spritesheet::DrawSprite(SDL_Rect *position) {
    SDL_RenderCopy(renderer, texture, &m_clip, position);

}