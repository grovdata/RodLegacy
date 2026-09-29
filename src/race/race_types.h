#pragma once

#include <cstdint>

namespace srrace {

constexpr int TRACK_SEGMENTS = 1120;   // 0x460
constexpr int DRAG_FINISH_SEG = 380;   // 0x17c, race type 0
constexpr int ROAD_FINISH_SEG = 1020;  // 0x3fc, race type 1
constexpr int RACE_START_SEG = 200;

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
};

inline void RoadEdges(uint16_t flags, int16_t& left, int16_t& right) {
    if ((flags & 0xe0) == 0)      { left = 1500; right = 3300; }
    else if (flags & 0x80)        { left = 2000; right = 2800; }
    else if (flags & 0x20)        { left = 2000; right = 3300; }
    else { left = 1500; right = 2800; }
}

}
