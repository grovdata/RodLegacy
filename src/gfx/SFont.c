/*  SFont: a simple font-library that uses special .pngs as fonts
    Copyright (C) 2003 Karl Bartel

    License: GPL or LGPL (at your choice)
    WWW: http://www.linux-games.com/sfont/

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Karl Bartel
    karl@karl.berlin
*/
#include <SDL.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "gfx/SFont.h"

static Uint32 GetPixel(SDL_Surface *Surface, Sint32 X, Sint32 Y)
{
   Uint8  *bits;
   Uint32 Bpp;

   assert(X>=0);
   assert(X<Surface->w);

   Bpp = Surface->format->BytesPerPixel;
   bits = ((Uint8 *)Surface->pixels)+Y*Surface->pitch+X*Bpp;

   /* Get the pixel */
   switch(Bpp) {
      case 1:
         return *((Uint8 *)Surface->pixels + Y * Surface->pitch + X);
         break;
      case 2:
         return *((Uint16 *)Surface->pixels + Y * Surface->pitch/2 + X);
         break;
      case 3: { /* Format/endian independent  */
         Uint8 r, g, b;
         r = *((bits)+Surface->format->Rshift/8);
         g = *((bits)+Surface->format->Gshift/8);
         b = *((bits)+Surface->format->Bshift/8);
         return SDL_MapRGB(Surface->format, r, g, b);
         }
         break;
      case 4:
         return *((Uint32 *)Surface->pixels + Y * Surface->pitch/4 + X);
         break;
   }

   return -1;
}

SFont_Font* SFont_InitFont(SDL_Surface* Surface)
{
    int x = 0, i = 0;
    Uint32 pixel;
    SFont_Font* Font;
    Uint32 pink;

    if (Surface == NULL)
	return NULL;

    Font = (SFont_Font *) malloc(sizeof(SFont_Font));
    Font->Surface = Surface;

    SDL_LockSurface(Surface);

    pink = SDL_MapRGB(Surface->format, 255, 0, 255);
    while (x < Surface->w) {
        if (GetPixel(Surface, x, 0) == pink) {
            Font->CharPos[i++]=x;
            while((x < Surface->w) && (GetPixel(Surface, x, 0)== pink))
                x++;
            Font->CharPos[i++]=x;
        } else {
        x++;
        }
    }
    // printf("Number of chars: %d\n", i);
    Font->MaxPos = x-1;
    pixel = GetPixel(Surface, 0, Surface->h-1);
    SDL_UnlockSurface(Surface);
    SDL_SetColorKey(Surface, 0, pixel);

    return Font;
}

void SFont_FreeFont(SFont_Font* FontInfo)
{
    SDL_FreeSurface(FontInfo->Surface);
    free(FontInfo);
}

void SFont_DumpFont(SFont_Font* Font)
{
    const char* chars = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    for (size_t i = 0; i < strlen(chars); i++) {
        char c = chars[i];
        int size = SFont_TextWidth(Font, &c);

        printf("%c %d\n", chars[i], size);
    }
}

void SFont_Write(SDL_Surface *Surface, const SFont_Font *Font,
		 int x, int y, const char *text)
{
    const char* c;
    int charoffset;
    SDL_Rect srcrect, dstrect;

    if(text == NULL)
	return;

    /* these values won't change in the loop */
    srcrect.y = 1;
    dstrect.y = y;
    srcrect.h = dstrect.h = Font->Surface->h - 1;

    for(c = text; *c != '\0' && x <= Surface->w ; c++) {
        charoffset = ((int) (*c - 33)) * 2 + 1;
        /* skip spaces and nonprintable characters */
        if (*c == ' ') {
            x += 5;
            continue;
        }

        if (*c == ' ' || charoffset < 0 || charoffset > Font->MaxPos) {
            x += Font->CharPos[2]-Font->CharPos[1];
            continue;
        }

        srcrect.w = dstrect.w =
            (Font->CharPos[charoffset+2] + Font->CharPos[charoffset+1])/2 -
            (Font->CharPos[charoffset] + Font->CharPos[charoffset-1])/2;
        srcrect.x = (Font->CharPos[charoffset]+Font->CharPos[charoffset-1])/2;
        dstrect.x = x - (float)(Font->CharPos[charoffset]
                    - Font->CharPos[charoffset-1])/2;

        SDL_BlitSurface(Font->Surface, &srcrect, Surface, &dstrect);
        // printf("src: %s %d %d %d %d\n", c, srcrect.x, srcrect.y, srcrect.w, srcrect.h);
        // printf("dst: %s %d %d %d %d\n", c, dstrect.x, dstrect.y, dstrect.w, dstrect.h);
        x += Font->CharPos[charoffset+1] - Font->CharPos[charoffset];
    }
}

int SFont_TextWidth(const SFont_Font *Font, const char *text)
{
    const char* c;
    int charoffset=0;
    int width = 0;

    if(text == NULL)
	return 0;

    for(c = text; *c != '\0'; c++) {
        /* Match SFont_Write's advances exactly, or a surface sized from this
         * width will clip the text. Write advances a space by a fixed 5px
         * (spaces have no glyph in the font), not by CharPos[2]-CharPos[1]. */
        if (*c == ' ') {
            width += 5;
            continue;
        }
	charoffset = ((int) *c - 33) * 2 + 1;
	/* nonprintable characters */
        if (charoffset < 0 || charoffset > Font->MaxPos) {
            width += Font->CharPos[2]-Font->CharPos[1];
	    continue;
	}

	width += Font->CharPos[charoffset+1] - Font->CharPos[charoffset];
    }

    return width;
}

int SFont_TextHeight(const SFont_Font* Font)
{
    return Font->Surface->h - 1;
}

void SFont_WriteCenter(SDL_Surface *Surface, const SFont_Font *Font,
		       int y, const char *text)
{
    SFont_Write(Surface, Font, Surface->w/2 - SFont_TextWidth(Font, text)/2,
	    	y, text);
}

SFont_Font* SFont_InitFontDirect(
    const uint8_t* pixels, int pixel_row_stride,
    const int* x_offsets, const int* glyph_widths, const int* advance_widths,
    int first_char, int num_chars, int ysize,
    uint8_t fg_r, uint8_t fg_g, uint8_t fg_b)
{
    int i, row, px;

    if (!pixels || num_chars <= 0 || ysize <= 0 || first_char < 33)
        return NULL;

    /* Surface width: one 1-pixel pink marker per char + advance pixels per char
     * + one sentinel pink marker at the end. */
    int surf_w = 1; /* sentinel */
    for (i = 0; i < num_chars; i++)
        surf_w += 1 + advance_widths[i];
    int surf_h = ysize + 1; /* row 0: pink markers; rows 1..ysize: glyphs */

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, surf_w, surf_h, 32,
                                                       SDL_PIXELFORMAT_RGBA8888);
    if (!surf)
        return NULL;

    /* Fill fully transparent */
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, 0, 0, 0, 0));

    SDL_LockSurface(surf);

    Uint32 pink        = SDL_MapRGBA(surf->format, 255, 0, 255, 255);
    Uint32 fg_color    = SDL_MapRGBA(surf->format, fg_r, fg_g, fg_b, 255);
    Uint32 transparent = SDL_MapRGBA(surf->format, 0, 0, 0, 0);

    int sx = 0; /* current x position in the surface */
    for (i = 0; i < num_chars; i++) {
        /* Row 0: pink marker (1 pixel wide) */
        Uint32* r0 = (Uint32*)((Uint8*)surf->pixels + 0 * surf->pitch);
        r0[sx] = pink;

        /* Rows 1..ysize: glyph pixels */
        int gw   = glyph_widths[i];
        int xoff = x_offsets[i];
        for (row = 0; row < ysize; row++) {
            Uint32* rn = (Uint32*)((Uint8*)surf->pixels + (row + 1) * surf->pitch);
            for (px = 0; px < gw; px++) {
                uint8_t bit = pixels[row * pixel_row_stride + xoff + px];
                rn[sx + 1 + px] = bit ? fg_color : transparent;
            }
        }

        sx += 1 + advance_widths[i];
    }

    /* Sentinel pink at end */
    Uint32* r0 = (Uint32*)((Uint8*)surf->pixels + 0 * surf->pitch);
    r0[sx] = pink;

    SDL_UnlockSurface(surf);

    /* SFont_InitFont scans row 0 for pink runs and builds CharPos[]. */
    SFont_Font* font = SFont_InitFont(surf);
    if (!font)
        SDL_FreeSurface(surf);
    return font;
}
