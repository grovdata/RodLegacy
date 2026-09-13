#pragma once

#include "gfx/bitmap.h"
#include <memory>

struct RectangleConfig {
    SDL_Rect rect;
    SDL_Color fillColor;
    SDL_Color borderColor;
    int inset;
};

class Rectangle: public IDrawable {
    private:
        RectangleConfig config;
        sdl2::SurfacePtr surface;
        sdl2::TexturePtr texture;

        bool visible;

        void Fill();

    public:          
        Rectangle(RectangleConfig config = { {0, 0, 0, 0}, {255, 255, 255, 255}, {0, 0, 0, 255}, 0 });
        void Draw();
        void SetVisible(bool visible);
        SDL_Surface* GetSurface() { return surface.get(); }
};