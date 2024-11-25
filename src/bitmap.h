#pragma once

#include "global.h"
#include "parsers/libp.h"

enum BitmapAlign {
    BitmapAlignBottom,
    BitmapAlignTop
};

class Bitmap {
    protected:
        SDL_Rect rect;
        bool isVisible;
        SDL_Texture *texture;
        SDL_Surface *surface;
    public:
        Bitmap();
        ~Bitmap();

        void     LoadBitmap(std::string filename);
        void     LoadSurface(SDL_Surface* src);
        void     LoadBitmapFromChunky(const vector<Color>& colors, int width, int height);
        void     SaveBitmap(int index);

        void     Align(BitmapAlign b, bool allowStatusBar);
        void     AlignAbove(Bitmap *dest);

        void     Crop(SDL_Rect crop);
        void     SetDimensions(int width, int height);
        SDL_Rect GetPosition();
        void     SetPosition(int x, int y);

        bool     IsVisible() const { return isVisible; }
        void     SetVisible(bool visible);

        virtual void Draw();

        //Remove later
        SDL_Surface* GetSurface();

        void UpdateFromSurface();
};
