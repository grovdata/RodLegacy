#pragma once

#include "global.h"

enum BitmapAlign {
    BitmapAlignBottom,
    BitmapAlignTop
};

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

        void Align(BitmapAlign b, bool allowStatusBar);
        void AlignAbove(Bitmap *dest);
        
        SDL_Rect GetPosition();
        void SetPosition(int x, int y);
        
        void SetVisible(bool visible);

        void Draw();
};
