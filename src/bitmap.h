#pragma once

#include "global.h"
#include "parsers/libp.h"

enum BitmapAlign {
    BitmapAlignBottom,
    BitmapAlignTop
};

class IDrawable {
    protected:
        bool visible;
    public:
        virtual ~IDrawable() = default;
        virtual void Draw() = 0;
        virtual void SetVisible(bool visible) = 0;
};

class Bitmap {
    protected:
        SDL_Rect rect;
        bool isVisible;
        SDL_Texture *texture;
        SDL_Surface *surface;
    public:
        Bitmap();
        Bitmap(int width, int height, int depth);
        virtual ~Bitmap();

        void     LoadBitmap(std::string filename);
        void     LoadSurface(SDL_Surface* src);
        void     LoadBitmapFromChunky(const vector<Color>& colors, int width, int height, bool isMask = false);
        void     SaveBitmap(int index);
        void     CreateMask(SDL_Surface *mask, SDL_Surface *target, SDL_Rect rect);

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
        SDL_Texture* GetTexture();

        void UpdateFromSurface();

        // Assignment from pointer
        Bitmap& operator=(const Bitmap* other) {
            if (other) {
                // Clean up existing resources
                if (texture) {
                    SDL_DestroyTexture(texture);
                }
                if (surface) {
                    SDL_FreeSurface(surface);
                }

                // Copy from other
                if (other->surface) {
                    surface = SDL_CreateRGBSurfaceFrom(other->surface->pixels,
                                                     other->surface->w,
                                                     other->surface->h,
                                                     other->surface->format->BitsPerPixel,
                                                     other->surface->pitch,
                                                     other->surface->format->Rmask,
                                                     other->surface->format->Gmask,
                                                     other->surface->format->Bmask,
                                                     other->surface->format->Amask);
                    texture = SDL_CreateTextureFromSurface(renderer, surface);
                }
                isVisible = other->isVisible;
                rect = other->rect;
            }
            return *this;
        }

        // Regular assignment operator
        Bitmap& operator=(const Bitmap& other) {
            if (this != &other) {
                // Clean up existing resources
                if (texture) {
                    SDL_DestroyTexture(texture);
                }
                if (surface) {
                    SDL_FreeSurface(surface);
                }

                // Copy from other
                if (other.surface) {
                    surface = SDL_CreateRGBSurfaceFrom(other.surface->pixels,
                                                     other.surface->w,
                                                     other.surface->h,
                                                     other.surface->format->BitsPerPixel,
                                                     other.surface->pitch,
                                                     other.surface->format->Rmask,
                                                     other.surface->format->Gmask,
                                                     other.surface->format->Bmask,
                                                     other.surface->format->Amask);
                    texture = SDL_CreateTextureFromSurface(renderer, surface);
                }
                isVisible = other.isVisible;
                rect = other.rect;
            }
            return *this;
        }
};
