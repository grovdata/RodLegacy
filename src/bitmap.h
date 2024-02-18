#pragma once

#include "global.h"

class Bitmap {
    private:        
        SDL_Surface *surface;
        SDL_Texture *texture;
        SDL_Rect rect;
    public:
        Bitmap();
        ~Bitmap();
        void LoadBitmap(std::string filename);

        void Draw();
};
