/*
 * RodLegacy - An image with its SDL surface and texture: Bitmap loading, palettes, crops and overlays.
 *
 * Created: 2024-02-18
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "parsers/libp.h"
#include "core/types.h"
#include "core/utils.h"

enum BitmapAlign {
    BitmapAlignBottom,
    BitmapAlignTop
};



class Bitmap : public IDrawable {
    protected:
        SDL_Rect rect;
        bool isVisible;
        sdl2::TexturePtr texture;
        sdl2::SurfacePtr surface;
        SDL_Rect sourceRect;
    public:
        Bitmap();
        Bitmap(int width, int height, int depth);
        Bitmap(const Bitmap& other);
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
        /** The loaded image's own size, unaffected by draw-time SetDimensions. */
        SDL_Rect GetNaturalSize();

        virtual void     SetPosition(int x, int y);
        void     SetSourceRect(const SDL_Rect& rect);
        SDL_Rect GetSourceRect() const { return sourceRect; }

        virtual bool     IsVisible() const { return isVisible; }

        virtual void     SetVisible(bool visible) override;

        virtual void Draw() override;
        void DrawWithRects(const SDL_Rect& src, const SDL_Rect& dst);

        SDL_Surface* GetSurface();
        SDL_Texture* GetTexture();

        void UpdateFromSurface();
        void ReplaceSurface(SDL_Surface* newSurface);

        Bitmap& operator=(const Bitmap* other);
        Bitmap& operator=(const Bitmap& other);
};
