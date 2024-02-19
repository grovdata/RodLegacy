#pragma once

#include "global.h"

class Bitmap {
    private:
        SDL_Texture *texture;
        SDL_Surface *surface;
    protected:
        SDL_Rect rect;
        bool isVisible;
    public:
        Bitmap();
        ~Bitmap();
        void LoadBitmap(std::string filename);
        void SetVisible(bool visible);

        void Draw();
};
