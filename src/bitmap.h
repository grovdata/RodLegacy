#pragma once

#include "global.h"
#include "parsers/libp.h"
#include "types.h"

enum BitmapAlign {
    BitmapAlignBottom,
    BitmapAlignTop
};



class Bitmap : public IDrawable {
    protected:
        SDL_Rect rect;
        bool isVisible;
        SDL_Texture *texture;
        SDL_Surface *surface;
        SDL_Rect sourceRect;
    public:
        Bitmap();
        Bitmap(int width, int height, int depth);
        Bitmap(const Bitmap& other);  // Copy constructor
        virtual ~Bitmap();

        void     LoadBitmap(std::string filename);
        void     LoadSurface(SDL_Surface* src);
        void     LoadBitmapFromChunky(const std::vector<SrLibColor>& colors, int width, int height, bool isMask = false, bool opaque = false);
        void     SaveBitmap(int index);
        void     CreateMask(SDL_Surface *mask, SDL_Surface *target, SDL_Rect rect);
        void     ApplyBumperOverlay(Bitmap* overlay, int offsetX, int offsetY);

        void     Align(BitmapAlign b, bool allowStatusBar);
        void     AlignAbove(Bitmap *dest);

        void     Crop(SDL_Rect crop);

        virtual void     SetDimensions(int width, int height);
        SDL_Rect GetPosition();

        virtual void     SetPosition(int x, int y);
        void     SetSourceRect(const SDL_Rect& rect);

        virtual bool     IsVisible() const { return isVisible; }

        virtual void     SetVisible(bool visible) override;

        virtual void Draw() override;

        //Remove later
        SDL_Surface* GetSurface();
        SDL_Texture* GetTexture();

        void UpdateFromSurface();

        Bitmap& operator=(const Bitmap* other);        
        Bitmap& operator=(const Bitmap& other);
};
