/*
 * RodLegacy - Shared race types: the track arrays, road edges and the race RNG.
 *
 * Created: 2026-07-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <cstdint>

namespace srrace {

constexpr int TRACK_SEGMENTS = 1120;   // 0x460
constexpr int DRAG_FINISH_SEG = 380;   // 0x17c
constexpr int ROAD_FINISH_SEG = 1020;  // 0x3fc
constexpr int RACE_START_SEG = 200;

// Which race is being run.
enum class RaceKind {
    Drag,
    Road,        // Street Rod only
    Mulholland,  // Street Rod 2 only
    Grudge,      // Street Rod 2 only
    Aqueduct,    // Street Rod 2 only, and a separate track
};

// Where on the road a race runs. The original keeps these in start_seg
// (A4+0x5e58) and finish_seg (A4+0x5e5a).
struct RaceWindow {
    int start;
    int finish;
};

inline RaceWindow RaceWindowFor(RaceKind kind) {
    switch (kind) {
        case RaceKind::Drag:       return {RACE_START_SEG, DRAG_FINISH_SEG};
        case RaceKind::Road:       return {RACE_START_SEG, ROAD_FINISH_SEG};
        case RaceKind::Mulholland: return {370, 960};
        case RaceKind::Grudge:     return {9, 89};
        case RaceKind::Aqueduct:   return {9, 343};
    }
    return {RACE_START_SEG, ROAD_FINISH_SEG};
}

// The original's race_type word (A4+0x2962) is a bit mask, and the engine tests combinations of it.
constexpr uint16_t RACE_MASK_CRUISE     = 0x01;
constexpr uint16_t RACE_MASK_DRAG       = 0x02;
constexpr uint16_t RACE_MASK_GRUDGE_A   = 0x04;
constexpr uint16_t RACE_MASK_GRUDGE_B   = 0x08;
constexpr uint16_t RACE_MASK_AQUEDUCT   = 0x10;
constexpr uint16_t RACE_MASK_MULHOLLAND = 0x20;

// Minimum gas to be allowed to start, checked against the car's tank.
inline int RaceMinimumGas(RaceKind kind) {
    switch (kind) {
        case RaceKind::Drag:       return 7;
        case RaceKind::Aqueduct:   return 12;
        case RaceKind::Mulholland: return 17;
        default:                   return 0;
    }
}

struct RaceRng {
    uint32_t state;
    explicit RaceRng(uint32_t seed = 0x2545f491u) : state(seed ? seed : 1u) {}
    int Next(int n) {
        state = state * 1664525u + 1013904223u;
        if (n <= 0) return 0;
        return static_cast<int>((state >> 16) % static_cast<uint32_t>(n));
    }
};

// flags[] bits: 0 city, 1 sign, 2 side street, 3 dash, 4 dash color,
//   5-7 road width, 8 sprite slot 5, 9/10 hill markers, 11 narrow warning,
//   12-15 overhead arch.
struct TrackData {
    uint16_t left[TRACK_SEGMENTS];
    uint16_t right[TRACK_SEGMENTS];
    uint16_t flags[TRACK_SEGMENTS];
    int16_t elev[TRACK_SEGMENTS];
    int16_t curve[TRACK_SEGMENTS];
    // Which lane across the road an on-road hazard sits in, 1..16, or 0 for none.
    int16_t lane[TRACK_SEGMENTS];
    // Shoulder offsets, Street Rod 2 only.
    int16_t wallLeft[TRACK_SEGMENTS];
    int16_t wallRight[TRACK_SEGMENTS];
};

// jdraw.c ProjectMirrorX 0x23aa86 / ProjectMirrorY 0x23aada, in the Amiga's 16-bit arithmetic.
inline int ProjectMirrorX(int playerLateral, int d, int lateral, int roadDelta) {
    const auto adjusted = static_cast<int16_t>(lateral + (static_cast<int16_t>(roadDelta * d) >> 2));
    if (d == 0) d = 1;
    const auto num = static_cast<int16_t>(playerLateral - adjusted + d / 2);
    return 280 - num / d;
}

inline int ProjectMirrorY(int d, int base, int rowC) {
    const auto v = static_cast<int16_t>(base + static_cast<int16_t>(rowC * d) - 1900);
    if (d == 0) d = 1;
    return 36 + static_cast<int16_t>(v + d / 2) / d;
}

// Street Rod 2's FUN_0024533e / FUN_002453e2: no rounding, and the camera height drops in a fall.
inline int ProjectMirrorXSr2(int playerLateral, int d, int lateral, int roadDelta) {
    const auto adjusted = static_cast<int16_t>(lateral + (static_cast<int16_t>(roadDelta * d) >> 2));
    if (d == 0) d = 1;
    return 280 - static_cast<int16_t>(playerLateral - adjusted) / d;
}

inline int ProjectMirrorYSr2(int d, int base, int rowC, int cameraY) {
    const auto v = static_cast<int16_t>(base + static_cast<int16_t>(rowC * d) - cameraY);
    if (d == 0) d = 1;
    return 36 + v / d;
}

inline void RoadEdges(uint16_t flags, int16_t& left, int16_t& right) {
    if ((flags & 0xe0) == 0)      { left = 1500; right = 3300; }
    else if (flags & 0x80)        { left = 2000; right = 2800; }
    else if (flags & 0x20)        { left = 2000; right = 3300; }
    else { left = 1500; right = 2800; }
}

// Street Rod 2. Collision (jroad.c FUN_0024644c) takes any width bit as 2000..2800. The renderer
// (FUN_00245536) draws a 0x20 or 0x40 row, the wet cement, with no road at all: both edges at 3300 or 1500.
inline void RoadEdgesSr2(uint16_t flags, bool draw, int16_t& left, int16_t& right) {
    if ((flags & 0xe0) == 0)          { left = 1500; right = 3300; }
    else if (!draw || (flags & 0x80)) { left = 2000; right = 2800; }
    else if (flags & 0x20)            { left = 3300; right = 3300; }
    else                              { left = 1500; right = 1500; }
}

}
