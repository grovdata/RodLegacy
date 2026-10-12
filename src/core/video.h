/*
 * RodLegacy - Window scale and fullscreen: whole-number scales of 320x200 that fit the display.
 *
 * Created: 2026-10-12
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <algorithm>

namespace video {

constexpr int kDefaultScale = 4;

// The largest whole-number scale whose window fits the usable area of the window's display.
int MaxScale();
// Pure part of MaxScale: usable area and title bar height in screen points, 320x200 = GAME_WIDTH x GAME_HEIGHT.
inline int MaxScaleFor(int usableW, int usableH, int titleBar) {
    const int s = std::min(usableW / 320, (usableH - titleBar) / 200);
    return s < 1 ? 1 : s;
}

int Scale();
// Clamped to 1..MaxScale(). Resizes and recentres the window; in fullscreen it takes effect on leaving it.
void SetScale(int scale);

bool Fullscreen();
void SetFullscreen(bool on);

// config.txt: window_scale (0 when unset) and fullscreen.
int SavedScale();
bool SavedFullscreen();
void Save();

}
