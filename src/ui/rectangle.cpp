#include "rectangle.h"

Rectangle::Rectangle(RectangleConfig config) : config(config) {
    Fill();
}

void Rectangle::Fill() {
    surface.reset(SDL_CreateRGBSurface(0, config.rect.w, config.rect.h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));

    SDL_Surface *s = surface.get();
    
    if (config.inset > 0) {
        SDL_FillRect(s, NULL, SDL_MapRGB(s->format, config.fillColor.r, config.fillColor.g, config.fillColor.b));
    }

    SDL_Rect border = {config.inset, config.inset, config.rect.w - config.inset * 2, config.rect.h - config.inset * 2};
    SDL_FillRect(s, &border, SDL_MapRGB(s->format, config.borderColor.r, config.borderColor.g, config.borderColor.b));
    
    
    SDL_Rect inner = {config.inset + 1, config.inset + 1, config.rect.w - config.inset * 2 - 2, config.rect.h - config.inset * 2 - 2};
    SDL_FillRect(s, &inner, SDL_MapRGB(s->format, config.fillColor.r, config.fillColor.g, config.fillColor.b));
    
    texture.reset(SDL_CreateTextureFromSurface(renderer, s));
}

void Rectangle::Draw() {
    if (!visible) return;
    SDL_RenderCopy(renderer, texture.get(), NULL, &config.rect);
}

void Rectangle::SetVisible(bool visible) {
    this->visible = visible;
}