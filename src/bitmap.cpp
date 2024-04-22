#include "bitmap.h"

Bitmap::Bitmap() {
}

Bitmap::~Bitmap() {
    if (surface != NULL) {
        SDL_FreeSurface(surface);
    }
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
}

void Bitmap::SetPosition(int x, int y) {
    this->rect.x = x;
    this->rect.y = y;
}

void Bitmap::SetVisible(bool visible) {
    this->isVisible = visible;
}

void Bitmap::LoadBitmap(std::string filename) {
    surface = IMG_Load(strdup(filename.c_str()));
    if (surface == NULL) {
        SDL_Log("Bitmap::LoadBitmap error, could not load surface %s\n", filename.c_str());
        return;
    } else {
        SDL_LogDebug(0, "Loaded bitmap %s, size %d x %d\n", filename.c_str(), surface->w, surface->h);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;
}

void Bitmap::Draw() {
    if (isVisible)
        SDL_RenderCopy(renderer, texture, NULL, &rect);
}