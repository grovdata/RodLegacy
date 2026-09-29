#pragma once

#include <cstdint>
#include <vector>

#include "gfx/SFont.h"

// Glyphs used as scroll-list button icons in dialogs.
constexpr char MAIN_FONT_SCROLL_UP     = '\x5B'; // '[' single up arrow: scroll up one line
constexpr char MAIN_FONT_SCROLL_TOP    = '\x5C'; // '\' double up arrow: jump to top
constexpr char MAIN_FONT_SCROLL_DOWN   = '\x5D'; // ']' single down arrow: scroll down one line
constexpr char MAIN_FONT_SCROLL_BOTTOM = '\x5E'; // '^' double down arrow: jump to bottom

// nullptr if the font data is missing or bad; keep the existing font then.
SFont_Font* LoadMainFontFromExe(const std::vector<uint8_t>& srExeData);
