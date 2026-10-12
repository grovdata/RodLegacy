/*
 * RodLegacy - Street Rod 2's SR2_end: the rect table and frame script of the ending animation.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>
#include <vector>

struct Sr2EndRect {
    int w, h, x, y;
};

struct Sr2EndSprite {
    int rect, x, y;
};

struct Sr2EndFrame {
    std::vector<Sr2EndSprite> sprites;
    int hold;   // ticks of 20 ms
};

struct Sr2EndScript {
    std::vector<Sr2EndRect> rects;
    std::vector<Sr2EndFrame> frames;
};

// 504 bytes of 6-byte rects, then 4-byte entries up to a 0xff first byte.
// Returns false and leaves `out` empty when the data does not parse.
bool Sr2EndParse(const std::vector<uint8_t>& data, Sr2EndScript& out);
