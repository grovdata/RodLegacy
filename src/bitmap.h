#pragma once

#include "global.h"
#include "parsers/libp.h"

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
        void         LoadBitmap(std::string filename);
        void         LoadBitmapFromChunky(const vector<Color>& colors, int width, int height);

        void Align(BitmapAlign b, bool allowStatusBar);
        void AlignAbove(Bitmap *dest);

        void SetDimensions(int width, int height);
        SDL_Rect GetPosition();
        void SetPosition(int x, int y);

        void SetVisible(bool visible);

        void Draw();
};
