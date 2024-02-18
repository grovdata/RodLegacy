#include "bitmap.h"

Bitmap::Bitmap() {
}

Bitmap::~Bitmap() {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void Bitmap::LoadBitmap(std::string filename) {
    surface = IMG_Load(strdup(filename.c_str()));
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;
}

void Bitmap::Draw() {
    SDL_RenderCopy(renderer, texture, NULL, &rect);
}