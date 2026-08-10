#pragma once

#include <cstdint>
#include <vector>

#include "SFont.h"

// Special glyphs in the Amiga main font (see LoadMainFontFromExe) used as
// scroll-list button icons in dialogs instead of ordinary letters/digits.
// See RodLegacyResearch/docs/formats/amiga_sr_font.md for the glyph bitmaps.
constexpr char MAIN_FONT_SCROLL_UP     = '\x5B'; // '[' single up arrow: scroll up one line
constexpr char MAIN_FONT_SCROLL_TOP    = '\x5C'; // '\' double up arrow: jump to top
constexpr char MAIN_FONT_SCROLL_DOWN   = '\x5D'; // ']' single down arrow: scroll down one line
constexpr char MAIN_FONT_SCROLL_BOTTOM = '\x5E'; // '^' double down arrow: jump to bottom

// Loads the Amiga main font embedded in the Street_Rod executable.
// Returns nullptr (caller should keep any existing/embedded font) if
// srExeData is too small or the font data fails to parse.
SFont_Font* LoadMainFontFromExe(const std::vector<uint8_t>& srExeData);
