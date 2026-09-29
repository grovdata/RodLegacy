/*
 * RodLegacy - SFont, a bitmap-font library for SDL by Karl Bartel, extended with SFont_InitFontDirect for
 * fonts built in memory.
 *
 * Created: 2023-02-05
 * License: the original author's terms below (GPL-2.0-or-later or LGPL, at your choice).
 */

/*  SFont: a simple font-library that uses special bitmaps as fonts
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
    Cecilienstr. 14
    12307 Berlin
    GERMANY
    karlb@gmx.net
*/

/************************************************************************
 *    SFONT - SDL Font Library by Karl Bartel <karl@karl.berlin>         *
 *                                                                       *
 *  All functions are explained below. For further information, take a   *
 *  look at the example files, the links at the SFont web site, or       *
 *  contact me, if you problem isn' addressed anywhere.                  *
 *                                                                       *
 ************************************************************************/
#ifndef SFONT_H
#define SFONT_H

#include <SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
** Declare one variable of this type for each font you are using.
** To load the fonts, load the font image into YourFont->Surface
** and call InitFont( YourFont );
*/
typedef struct {
    SDL_Surface *Surface;
    int CharPos[512];
    int MaxPos;
} SFont_Font;

/*
** Initializes the font
** Font: this contains the suface with the font.
**       The Surface must be loaded before calling this function
*/
SFont_Font *SFont_InitFont(SDL_Surface *Font);

/*
** Frees the font
** Font: The font to free
**       The font must be loaded before using this function.
*/
void SFont_FreeFont(SFont_Font *Font);

/* Font debug */
void SFont_DumpFont(SFont_Font *Font);

/*
** Blits a string to a surface
** Destination: the surface you want to blit to
** text: a string containing the text you want to blit.
*/
void SFont_Write(SDL_Surface *Surface, const SFont_Font *Font, int x, int y, const char *text);

/* Returns the width of "text" in pixels */
int SFont_TextWidth(const SFont_Font *Font, const char *text);
/* Returns the height of "text" in pixels (which is always equal to Font->Surface->h) */
int SFont_TextHeight(const SFont_Font *Font);

/* Blits a string to Surface with centered x position */
void SFont_WriteCenter(SDL_Surface *Surface, const SFont_Font *Font, int y, const char *text);

/*
** Initializes a font directly from raw in-memory glyph data, without a PNG.
**
** pixels:            byte-per-pixel array (1 = foreground, 0 = background),
**                    row-major, pixel_row_stride bytes per row.
** pixel_row_stride:  bytes per row in the pixels array (>= max x_offsets[i]+glyph_widths[i]).
** x_offsets:         x column in pixels[] where each glyph starts (num_chars entries).
** glyph_widths:      pixel width of each glyph (num_chars entries).
** advance_widths:    how far to advance x after printing each char (num_chars entries).
** first_char:        ASCII code of the first character (must be >= 33, i.e. '!').
**                    Space (0x20) is handled by SFont_Write internally and must not
**                    be included.
** num_chars:         number of characters.
** ysize:             glyph height in pixels.
** fg_r, fg_g, fg_b:  foreground colour for lit glyph pixels.
**
** Returns a newly allocated SFont_Font, or NULL on error.
** Caller is responsible for freeing it with SFont_FreeFont.
*/
SFont_Font* SFont_InitFontDirect(
    const uint8_t* pixels, int pixel_row_stride,
    const int* x_offsets, const int* glyph_widths, const int* advance_widths,
    int first_char, int num_chars, int ysize,
    uint8_t fg_r, uint8_t fg_g, uint8_t fg_b);

#ifdef __cplusplus
}
#endif

#endif /* SFONT_H */
