#pragma once
// Shared by racing.cpp, racing_render.cpp and racing_debug.cpp.

#include <SDL.h>

#include <algorithm>

#include "parsers/libp.h"
#include "race/race_sim.h"

namespace racing_internal {

inline constexpr int kForwardRows = 15;
inline constexpr int kTrackSegments = srrace::TRACK_SEGMENTS;

// This surface lands at screen y = 14, so every screen-space Y from the original is converted.
inline constexpr int kViewOffsetY = 14;
inline constexpr int kHorizonY = 60 - kViewOffsetY;    // ProjectForwardY base, screen 60
inline constexpr int kClipTop = 18 - kViewOffsetY;     // forward viewport clip, screen 18..99
inline constexpr int kClipBottom = 99 - kViewOffsetY;
inline constexpr int kRoadCenterLat = 2400;

// Races run at night. The grass is the backdrop's foothill green, so they read as one field.
inline constexpr SDL_Color kSkyColor = {0, 0, 0, 255};
// Town walls, PAL_DRIVING entries 2, 4 and 10. Cruise only.
inline constexpr SDL_Color kTownWallGrey = {0x99, 0x99, 0x99, 255};   // PAL_DRIVING[2]
inline constexpr SDL_Color kTownWallDark = {0x33, 0x33, 0x33, 255};   // PAL_DRIVING[4]
inline constexpr SDL_Color kTownUpper = {0xEE, 0xEE, 0xEE, 255};      // PAL_DRIVING[10]
inline constexpr SDL_Color kGrassA = {0, 112, 0, 255};
inline constexpr SDL_Color kRoadA = {0, 0, 0, 255};
inline constexpr SDL_Color kDashWhite = {235, 235, 235, 255};
inline constexpr SDL_Color kDashYellow = {235, 215, 40, 255};
inline constexpr SDL_Color kCurbGray = {170, 170, 170, 255};
inline constexpr SDL_Color kArchColor = {180, 32, 32, 255};  // overhead gantry: red

// Rendering stays at GAME_FPS and samples the sim, which steps at the race speed setting.
inline float StepSeconds() {
    return 1.0f / static_cast<float>(srrace::SimHz());
}

inline int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

// sr grows with distance, 0 = nearest. Frames may be authored either way, so the nearest is the largest by area.
inline int SelectFrameIndexForDistance(const SpriteAnimationInfo* anim, int sr) {
    const int last = anim->frameCount - 1;
    const int srClamped = ClampInt(sr, 0, last);
    const SpriteRect& f0 = anim->frames[0];
    const SpriteRect& fN = anim->frames[last];
    const bool smallestFirst = (fN.w * fN.h) > (f0.w * f0.h);
    return smallestFirst ? (last - srClamped) : srClamped;
}

}
