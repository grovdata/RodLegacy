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

SDL_Rect Bitmap::GetPosition() {
    return this->rect;
}

void Bitmap::SetPosition(int x, int y) {
    this->rect.x = x;
    this->rect.y = y;
}

void Bitmap::Align(BitmapAlign b, bool allowStatusBar) {
    int offset = allowStatusBar ? GAME_STATUSBAR_HEIGHT : 0;
    if (b == BitmapAlignTop) {
        this->rect.y = 0 + offset;
    } else {
        this->rect.y = GAME_HEIGHT - this->rect.h - offset;
    }
}

void Bitmap::AlignAbove(Bitmap *dest) {
    this->rect.y = dest->GetPosition().y - this->rect.h;
}

void Bitmap::SetVisible(bool visible) {
    this->isVisible = visible;
}

void Bitmap::LoadBitmap(std::string filename) {
    surface = IMG_Load(strdup(filename.c_str()));
    SDL_SetColorKey(surface, 0, SDL_MapRGB(surface->format, 0, 0, 0));
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
