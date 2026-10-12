#include "race/track_gen.h"

#include <algorithm>
#include <cstring>

namespace srrace {
namespace {

// curve[] renders as left/right curves, elev[] as hills. The original labels these the opposite way;
// names here follow what is on screen. ShapeCurveLeft/Right set the turn-sign flags 0x0200/0x0400.

constexpr int GEN_START = 390;

// +-5 hill profiles copied into elev[] (rise to a crest, fall to a dip).
constexpr int16_t SHALLOW[] = {
    1, 2, 3, 4, 4, 4, 4, 3, 2, 1, 0, 0,
    -1, -2, -3, -4, -4, -4, -4, -3, -2, -1,
};

constexpr int16_t DEEP[] = {
    1, 2, 3, 4, 4, 4, 4, 3, 2, 1, 0, 0,
    -1, -2, -3, -4, -5, -5, -5, -5, -5, -5,
    -4, -3, -2, -1, 0, 0, 1, 2, 3, 4, 4, 4, 4, 3, 2, 1,
};

constexpr int16_t GENTLE[] = {
    1, 2, 3, 3, 3, 2, 1, 0, 0, 0, -1, -2, -3, -3, -3, -2, -1,
};

constexpr int MAX_SHAPES_PER_RUN = 13;

// Per-variant shape sequences, one row per profile.
int SHAPE_SEQUENCES[2][10][MAX_SHAPES_PER_RUN] = {
    {   // Street Rod
        {2, 3, 1, 7, 4, 10, 5, 8, 6, 2, 9, 1, 0},
        {11, 4, 8, 1, 6, 9, 2, 5, 7, 2, 3, 1, 0},
        {1, 5, 1, 3, 8, 11, 6, 7, 2, 4, 9, 1, 0},
        {3, 1, 4, 9, 2, 6, 8, 1, 3, 8, 10, 2, 0},
        {10, 9, 5, 1, 6, 2, 3, 7, 4, 2, 8, 2, 0},
        {2, 3, 8, 10, 5, 11, 4, 7, 1, 6, 9, 2, 0},
        {1, 5, 9, 1, 4, 2, 3, 10, 8, 4, 1, 9, 0},
        {2, 4, 7, 11, 2, 5, 8, 4, 1, 7, 6, 2, 0},
        {2, 6, 8, 10, 6, 1, 3, 7, 5, 1, 4, 9, 0},
        {1, 3, 1, 6, 1, 4, 9, 10, 6, 8, 2, 3, 0},
    },
    {   // Street Rod 2
        {2, 3, 1, 4, 7, 10, 5, 8, 6, 2, 9, 1, 2},
        {11, 4, 1, 6, 8, 9, 2, 5, 7, 2, 3, 1, 2},
        {1, 5, 1, 3, 8, 11, 6, 7, 2, 4, 9, 1, 2},
        {3, 1, 4, 9, 2, 8, 6, 1, 3, 8, 10, 2, 1},
        {9, 10, 5, 1, 6, 2, 3, 7, 4, 2, 8, 2, 1},
        {2, 3, 8, 10, 5, 11, 4, 7, 1, 6, 9, 2, 1},
        {1, 5, 9, 1, 4, 2, 3, 10, 8, 4, 1, 9, 1},
        {2, 4, 7, 11, 2, 5, 8, 4, 1, 7, 6, 2, 1},
        {2, 6, 8, 10, 6, 1, 3, 7, 5, 1, 4, 9, 2},
        {1, 3, 1, 6, 1, 4, 9, 10, 6, 8, 2, 3, 1},
    },
};

constexpr TrackGenParams kStreetRodParams  = {1120, 0x442, 12, 3, 0x1801, false, false};
constexpr TrackGenParams kStreetRod2Params = { 990, 0x3de, 13, 7, 0x9811, true,  true};

// Where the rolling hills start, and one delta per block of this many.
constexpr int HILL_START = 380;
constexpr int HILL_BLOCK = 10;
constexpr int HILL_COUNT = 61;
// The run-in to the finish is held flat, as are the narrow crossings.
constexpr int HILL_FLAT_FROM = 920;
constexpr int HILL_FLAT_TO   = 960;
constexpr uint16_t HILL_BLOCKED_FLAG = 0x0080;

// Fallback for the table below, read from the executable by LoadTrackTables.
int16_t HILL_PROFILE[HILL_COUNT] = {
     1, -2,  2, -2,  2, -2,  2, -1,  0,  1,
     0,  1,  0, -1,  0,  1,  0,  1, -1,  2,
     1,  0,  1, -1,  2, -1,  0,  1, -1,  1,
     1, -1,  0, -1,  1,  0,  1,  2, -1,  0,
     0,  1, -1,  2, -1,  2, -1,  0,  1,  1,
     0, -1,  0,  1,  1, -1,  0, -1,  1,  0,
     1,
};

// Where each game keeps its variant table, as a big-endian word array.
constexpr size_t kStreetRodVariantsAt  = 0x2C3B2;
constexpr size_t kStreetRod2VariantsAt = 0x3FF02;
constexpr size_t kStreetRod2HillsAt    = 0x40006;

// The generator is one synchronous pass, so the shape functions read the knobs
// from here rather than taking them through thirty call sites.
const TrackGenParams* gShape = &kStreetRodParams;
const int (*gVariantRows)[MAX_SHAPES_PER_RUN] = SHAPE_SEQUENCES[0];

int GenLimit() { return gShape->genLimit; }

struct GenState {
    int gen_pos = GEN_START;
    int cur_curve = 0;
};

bool InLimit(int pos) {
    return pos >= 0 && pos < GenLimit();
}

void Advance(GenState& state, int amount) {
    state.gen_pos += amount;
}

void ClearAll(TrackData& track, int i) {
    track.left[i] = 0;
    track.right[i] = 0;
    track.flags[i] = 0;
    track.elev[i] = 0;
    track.curve[i] = 0;
}

// The original's re-clear keeps what RoadLayoutInit placed: arch/gantry nibble, sprite slot 5, line colour,
// city/sign/side street, and roadside scenery. It drops width, dash, curve/narrow markers and barriers.
void ClearGeneratedSegment(TrackData& track, int i, int roadType) {
    track.elev[i] = 0;
    track.curve[i] = 0;
    track.flags[i] &= 0xf117;
    track.left[i] &= 0xe7ff;
    track.right[i] &= 0x87ff;
    const int spacing = roadType * 4 + 4;
    if (spacing > 0 && (i + 1) % spacing == 0) {
        track.flags[i] |= 0x0008;
    }
}

void ClearObjects(TrackData& track, int pos) {
    if (InLimit(pos)) {
        track.left[pos] &= gShape->objectMask;
        track.right[pos] &= gShape->objectMask;
    }
}

void BackfillCurve(TrackData& track, int start, int end, int curve) {
    for (int i = start; i < end; ++i) {
        if (InLimit(i)) {
            track.curve[i] = static_cast<int16_t>(curve);
        }
    }
}

void ShapeCurveLeft(TrackData& track, GenState& state) {
    const int pos = state.gen_pos;
    const int curCurve = state.cur_curve;
    for (int i = pos; i < pos + 50; ++i) {
        if (InLimit(i)) {
            track.curve[i] = static_cast<int16_t>(curCurve);
        }
    }
    if (InLimit(pos)) {
        track.flags[pos] |= 0x0200;
    }
    Advance(state, 5);
    ClearObjects(track, state.gen_pos);
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] |= 0x0200;
    }
    Advance(state, 6);

    const int step = gShape->curveStep;
    int k = curCurve / step;
    const int end = k + 40;
    while (k < end && state.gen_pos < GenLimit()) {
        track.flags[state.gen_pos] |= 0x0008;
        track.curve[state.gen_pos] = static_cast<int16_t>(k * step);
        ++state.gen_pos;
        ++k;
    }
    state.cur_curve = k * step;
}

void ShapeCurveRight(TrackData& track, GenState& state) {
    const int pos = state.gen_pos;
    const int curCurve = state.cur_curve;
    for (int i = pos; i < pos + 50; ++i) {
        if (InLimit(i)) {
            track.curve[i] = static_cast<int16_t>(curCurve);
        }
    }
    if (InLimit(pos)) {
        track.flags[pos] |= 0x0400;
    }
    Advance(state, 3);
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] |= 0x0400;
    }
    Advance(state, 3);

    const int step = gShape->curveStep;
    int k = curCurve / step;
    const int end = k - 45;
    while (k > end && state.gen_pos < GenLimit()) {
        track.flags[state.gen_pos] |= 0x0008;
        track.curve[state.gen_pos] = static_cast<int16_t>(k * step);
        ++state.gen_pos;
        --k;
    }
    state.cur_curve = k * step;
}

void CopyHillTable(TrackData& track, GenState& state, const int16_t* table, int count, bool negate) {
    int idx = 0;
    while (idx < count && state.gen_pos < GenLimit()) {
        const int pos = state.gen_pos;
        ++state.gen_pos;
        track.elev[pos] = negate ? static_cast<int16_t>(-table[idx]) : table[idx];
        ++idx;
    }
}

void ShapeHillShallow(TrackData& track, GenState& state, bool negate) {
    const int start = state.gen_pos;
    Advance(state, negate ? 10 : 20);
    CopyHillTable(track, state, SHALLOW, static_cast<int>(sizeof(SHALLOW) / sizeof(SHALLOW[0])), negate);
    Advance(state, negate ? 18 : 8);
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

void ShapeHillDeep(TrackData& track, GenState& state, bool negate) {
    const int start = state.gen_pos;
    Advance(state, negate ? 8 : 6);
    CopyHillTable(track, state, DEEP, static_cast<int>(sizeof(DEEP) / sizeof(DEEP[0])), negate);
    Advance(state, negate ? 4 : 6);
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

void ShapeNarrowRight(TrackData& track, GenState& state) {
    const int start = state.gen_pos;
    Advance(state, 3);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x0800;
    }
    Advance(state, 4);
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] &= 0x0ff7;
    }
    Advance(state, 1);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x1000;
    }
    Advance(state, 1);
    for (int i = 0; i < 10; ++i) {
        if (InLimit(state.gen_pos)) {
            track.flags[state.gen_pos] |= 0x0040;
            track.flags[state.gen_pos] &= 0x0ff7;
        }
        Advance(state, 1);
    }
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] &= 0x0ff7;
    }
    Advance(state, 4);
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

void ShapeNarrowLeft(TrackData& track, GenState& state) {
    const int start = state.gen_pos;
    Advance(state, 4);
    if (InLimit(state.gen_pos)) {
        track.left[state.gen_pos] |= 0x0800;
    }
    Advance(state, 4);
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] &= 0x0ff7;
    }
    Advance(state, 1);
    if (InLimit(state.gen_pos)) {
        track.left[state.gen_pos] |= 0x1000;
    }
    Advance(state, 1);
    for (int i = 0; i < 10; ++i) {
        if (InLimit(state.gen_pos)) {
            track.flags[state.gen_pos] |= 0x0020;
            track.flags[state.gen_pos] &= 0x0ff7;
        }
        Advance(state, 1);
    }
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] &= 0x0ff7;
    }
    Advance(state, 3);
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

// jtown.c FUN_0024bd98 / FUN_0024c09a: Street Rod 2's narrows, laid only where the two walls differ. A road
// work sign, then a 0x1000 marker, then five segments of wet cement (flags 0x40 right, 0x20 left) with the dash,
// bit 2 and the arch nibble cleared and the objects beside them dropped.
void ShapeNarrowSr2(TrackData& track, GenState& state, bool right) {
    const int start = state.gen_pos;
    if (!InLimit(start) || track.wallLeft[start] == track.wallRight[start]) return;
    uint16_t* signSide = right ? track.right : track.left;
    uint16_t* otherSide = right ? track.left : track.right;
    Advance(state, right ? 3 : 4);
    if (InLimit(state.gen_pos)) signSide[state.gen_pos] |= 0x0800;
    if (InLimit(state.gen_pos + 4)) track.flags[state.gen_pos + 4] &= 0x0ff7;
    Advance(state, 5);
    if (InLimit(state.gen_pos)) {
        otherSide[state.gen_pos] &= 0xefee;
        signSide[state.gen_pos] |= 0x1000;
    }
    Advance(state, 1);
    for (int i = 0; i < 5 && state.gen_pos < GenLimit(); ++i) {
        const int p = state.gen_pos;
        track.flags[p] |= right ? 0x0040 : 0x0020;
        track.flags[p] &= 0x0ff5;
        track.right[p] &= 0xefee;
        track.left[p] &= 0xefee;
        Advance(state, 1);
    }
    if (InLimit(state.gen_pos)) track.flags[state.gen_pos] &= 0x0ff5;
    Advance(state, right ? 4 : 3);
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

void ShapeNarrowBoth(TrackData& track, GenState& state) {
    const int start = state.gen_pos;
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] |= 0x0008;
    }
    Advance(state, 4);
    ClearObjects(track, state.gen_pos);
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] |= 0x0008;
    }
    Advance(state, 6);
    for (int i = 0; i < 20; ++i) {
        if (InLimit(state.gen_pos)) {
            track.flags[state.gen_pos] |= 0x0080;
            track.flags[state.gen_pos] &= 0x0ff7;
        }
        Advance(state, 1);
    }
    if (InLimit(state.gen_pos)) {
        track.flags[state.gen_pos] &= 0x0fff;
    }
    BackfillCurve(track, start, state.gen_pos, state.cur_curve);
}

int SlopeSteps(TrackData& track, GenState& state, int count, int step) {
    int k = state.cur_curve / 3;
    for (int i = 0; i < count; ++i) {
        if (state.gen_pos >= GenLimit()) {
            break;
        }
        track.flags[state.gen_pos] |= 0x0008;
        track.curve[state.gen_pos] = static_cast<int16_t>(k * 3);
        ++state.gen_pos;
        k += step;
    }
    return k * 3;
}

void ShapeCurveHump(TrackData& track, GenState& state) {
    const int pos = state.gen_pos;
    const int curCurve = state.cur_curve;
    for (int i = pos; i < pos + 90; ++i) {
        if (InLimit(i)) {
            track.curve[i] = static_cast<int16_t>(curCurve);
        }
    }
    Advance(state, 1);
    ClearObjects(track, state.gen_pos);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x2000;
    }
    Advance(state, 6);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x2000;
    }
    state.cur_curve = SlopeSteps(track, state, 40, 1);
    state.cur_curve = SlopeSteps(track, state, 43, -1);
}

void ShapeCurveDip(TrackData& track, GenState& state) {
    const int pos = state.gen_pos;
    const int curCurve = state.cur_curve;
    for (int i = pos; i < pos + 90; ++i) {
        if (InLimit(i)) {
            track.curve[i] = static_cast<int16_t>(curCurve);
        }
    }
    Advance(state, 1);
    ClearObjects(track, state.gen_pos);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x4000;
    }
    Advance(state, 6);
    if (InLimit(state.gen_pos)) {
        track.right[state.gen_pos] |= 0x4000;
    }
    state.cur_curve = SlopeSteps(track, state, 35, -1);
    state.cur_curve = SlopeSteps(track, state, 48, 1);
}

void RunShape(TrackData& track, GenState& state, int shapeId) {
    // Street Rod 2 kept eleven codes in its variant table but only seven shapes behind them.
    if (gShape->hillsAreCurves) {
        switch (shapeId) {
        case 4: case 6: shapeId = 1; break;
        case 3: case 5: shapeId = 2; break;
        default: break;
        }
    }
    switch (shapeId) {
    case 1: ShapeCurveLeft(track, state); break;
    case 2: ShapeCurveRight(track, state); break;
    case 3: ShapeHillShallow(track, state, false); break;
    case 4: ShapeHillShallow(track, state, true); break;
    case 5: ShapeHillDeep(track, state, false); break;
    case 6: ShapeHillDeep(track, state, true); break;
    case 7: gShape->hillsAreCurves ? ShapeNarrowSr2(track, state, true) : ShapeNarrowRight(track, state); break;
    case 8: gShape->hillsAreCurves ? ShapeNarrowSr2(track, state, false) : ShapeNarrowLeft(track, state); break;
    case 9: ShapeNarrowBoth(track, state); break;
    case 10: ShapeCurveHump(track, state); break;
    case 11: ShapeCurveDip(track, state); break;
    default: break;
    }
}

bool SafeSetBit(uint16_t* values, int index, uint16_t bit) {
    if (index < 0 || index >= TRACK_SEGMENTS) {
        return false;
    }
    values[index] |= bit;
    return true;
}

}

void BuildGrudgeStrip(TrackData& track) {
    for (int s = 0; s < GRUDGE_SEGMENTS; ++s) {
        const int from = GRUDGE_ROAD_FIRST + s;
        if (from >= TRACK_SEGMENTS) break;
        track.flags[s] = static_cast<uint16_t>(track.flags[from] | GRUDGE_DASH_BIT);
        track.left[s] = track.left[from];
        track.right[s] = track.right[from];
        track.curve[s] = track.curve[from];
        track.elev[s] = track.elev[from];
        track.lane[s] = track.lane[from];
        track.wallLeft[s] = track.wallLeft[from];
        track.wallRight[s] = track.wallRight[from];
    }
    track.flags[GRUDGE_FINISH_MARKER] |= GRUDGE_DASH_COLOUR_BIT;

    for (int s = GRUDGE_SEGMENTS; s < TRACK_SEGMENTS; ++s) {
        ClearAll(track, s);
        track.curve[s] = 0;
        track.elev[s] = 0;
        track.lane[s] = 0;
        track.wallLeft[s] = 0;
        track.wallRight[s] = 0;
    }
}

void RoadLayoutInit(TrackData& track, RaceRng& rng, int roadType, TrackGenSource source) {
    for (int i = 0; i < TRACK_SEGMENTS; ++i) {
        ClearAll(track, i);
    }

    const int dashSpacing = roadType * 4 + 4;

    for (int i = 0; i < 180; ++i) {
        ClearAll(track, i);
        track.flags[i] |= 0x0001;
        if (dashSpacing > 0 && (i + 1) % dashSpacing == 0) {
            track.flags[i] |= 0x0008;
        }

        if ((i + 2) % 16 == 0 && i + 3 < TRACK_SEGMENTS) {
            for (int j = i + 1; j <= i + 3; ++j) {
                ClearAll(track, j);
            }
            track.flags[i] |= 0x0004;
            track.flags[i + 1] |= 0x0005;
            track.flags[i + 2] |= 0x0005;
            track.flags[i + 3] |= 0x0001;
            SafeSetBit(track.right, i - 1, 0x0040);
            SafeSetBit(track.left, i + 3, 0x0040);
            i += 3;
            continue;
        }

        const bool leftBuilding = ((i + 4) % 16 == 0) || ((i + 8) % 16 == 0) || ((i + 12) % 16 == 0);
        if (leftBuilding) {
            if (rng.Next(2) == 0) {
                SafeSetBit(track.left, i - 1, 0x0004);
                track.left[i] |= 0x0004;
                if (rng.Next(3) == 0) {
                    SafeSetBit(track.left, i - 1, 0x0008);
                    track.left[i] |= 0x0008;
                }
                track.left[i] |= 0x0010;
            } else {
                SafeSetBit(track.left, i - 2, 0x0004);
                SafeSetBit(track.left, i - 1, 0x0004);
                track.left[i] |= 0x0004;
                if (rng.Next(3) == 0) {
                    SafeSetBit(track.left, i - 2, 0x0008);
                    SafeSetBit(track.left, i - 1, 0x0008);
                    track.left[i] |= 0x0008;
                }
                SafeSetBit(track.left, i - 1, 0x0010);
            }
        } else {
            track.left[i] |= 0x0080;
        }

        const bool rightBuilding = ((i + 5) % 16 == 0) || ((i + 9) % 16 == 0) || ((i + 12) % 16 == 0);
        if (rightBuilding) {
            if (rng.Next(3) != 0) {
                SafeSetBit(track.right, i - 1, 0x0004);
                track.right[i] |= 0x0004;
                if (rng.Next(2) == 0) {
                    SafeSetBit(track.right, i - 1, 0x0008);
                    track.right[i] |= 0x0008;
                }
                track.right[i] |= 0x0010;
            } else {
                SafeSetBit(track.right, i - 2, 0x0004);
                SafeSetBit(track.right, i - 1, 0x0004);
                track.right[i] |= 0x0004;
                if (rng.Next(4) == 0) {
                    SafeSetBit(track.right, i - 2, 0x0008);
                    SafeSetBit(track.right, i - 1, 0x0008);
                    track.right[i] |= 0x0008;
                }
                SafeSetBit(track.right, i - 1, 0x0010);
            }
        } else {
            track.right[i] |= 0x0080;
        }

        if (rng.Next(roadType * 40 + 6) == 0 && (track.left[i] & 0x0010) == 0) {
            track.left[i] |= 0x0020;
        }
        if (rng.Next(roadType * 40 + 7) == 0 && (track.right[i] & 0x0010) == 0) {
            track.right[i] |= 0x0020;
        }
        if (rng.Next(roadType * 40 + 3) == 0 && (track.left[i] & 0x0010) == 0) {
            track.left[i] |= 0x0100;
        }
        if (rng.Next(roadType * 40 + 4) == 0 && (track.right[i] & 0x0010) == 0) {
            track.right[i] |= 0x0100;
        }
    }

    // The original sets bits 6/7 of the word's high byte, i.e. 0x4000/0x8000.
    track.flags[10 + rng.Next(45)] |= 0x4000;
    track.flags[10 + rng.Next(45)] |= 0x8000;
    track.flags[65 + rng.Next(55)] |= 0x4000;
    track.flags[65 + rng.Next(55)] |= 0x8000;
    track.flags[115 + rng.Next(55)] |= 0x4000;
    track.flags[115 + rng.Next(55)] |= 0x8000;

    // The three buildings a cruise pulls up at are in both games.
    track.right[44] |= 0x0001;
    track.left[108] |= 0x0001;
    track.left[156] |= 0x0001;
    // The six roadside objects flanking them are Street Rod's only: SR2's
    // FUN_0024c7ec writes the three above and nothing else at fixed segments.
    if (source != TrackGenSource::StreetRod2) {
        track.left[29] |= 0x1000;
        track.left[24] |= 0x0800;
        track.left[87] |= 0x1000;
        track.left[82] |= 0x0800;
        track.left[135] |= 0x1000;
        track.left[130] |= 0x0800;
    }

    for (int i = 180; i < TRACK_SEGMENTS; ++i) {
        ClearAll(track, i);
        if (dashSpacing > 0 && (i + 1) % dashSpacing == 0) {
            track.flags[i] |= 0x0008;
        }
        if (dashSpacing > 0 && i % dashSpacing == 0) {
            track.left[i] |= 0x0001;
            track.right[i] |= 0x0001;
        }
        const int signSpacing = roadType * 8 + 8;
        if (signSpacing > 0 && (i + 2) % signSpacing == 0) {
            track.flags[i] |= 0x0002;
        }

        if (rng.Next(roadType * 100 + 7) == 0) {
            track.left[i] |= 0x0020;
        }
        if (rng.Next(roadType * 100 + 9) == 0) {
            track.right[i] |= 0x0020;
        }
        if (roadType == 0) {
            if (rng.Next(35) == 0) {
                track.left[i] |= 0x0040;
                if (i >= 1) track.left[i - 1] |= 0x0040;
                if (i >= 2) track.left[i - 2] |= 0x0040;
                track.left[i] &= static_cast<uint16_t>(~0x0020u);
                if (i >= 1) track.left[i - 1] &= static_cast<uint16_t>(~0x0020u);
                if (i >= 2) track.left[i - 2] &= static_cast<uint16_t>(~0x0020u);
            }
            if (rng.Next(25) == 0) {
                track.right[i] |= 0x0040;
                if (i >= 2) track.right[i - 2] |= 0x0040;
                track.right[i] &= static_cast<uint16_t>(~0x0020u);
                if (i >= 2) track.right[i - 2] &= static_cast<uint16_t>(~0x0020u);
            }
        } else {
            if (rng.Next(roadType * 75 + 35) == 0) {
                track.left[i] |= 0x0040;
                track.left[i] &= static_cast<uint16_t>(~0x0020u);
            }
            if (rng.Next(roadType * 75 + 25) == 0) {
                track.right[i] |= 0x0040;
                track.right[i] &= static_cast<uint16_t>(~0x0020u);
            }
        }
        if (rng.Next(roadType * 50 + 7) == 0 && (track.left[i] & 0x0080) == 0) {
            track.left[i] |= 0x0080;
        }
        if (rng.Next(roadType * 50 + 9) == 0 && (track.right[i] & 0x0080) == 0) {
            track.right[i] |= 0x0080;
        }
        if (rng.Next(roadType * 75 + 21) == 0) {
            track.left[i] |= 0x0004;
        }
        if (rng.Next(roadType * 75 + 25) == 0) {
            track.right[i] |= 0x0004;
        }
        if (rng.Next(roadType * 75 + 23) == 0 && (track.left[i] & 0x0004) == 0) {
            track.left[i] |= 0x0008;
        }
        if (rng.Next(roadType * 75 + 30) == 0 && (track.right[i] & 0x0004) == 0) {
            track.right[i] |= 0x0008;
        }
        if (rng.Next(roadType * 30 + 4) == 0) {
            track.left[i] |= 0x0100;
            if (rng.Next(2) == 0) {
                track.left[i] |= 0x0200;
            }
        }
        if (rng.Next(roadType * 30 + 4) == 0) {
            track.right[i] |= 0x0100;
            if (rng.Next(2) == 0) {
                track.right[i] |= 0x0200;
            }
        }
    }

    track.left[379] &= 0x1801;
    track.right[379] &= 0x1801;
    track.flags[379] |= 0x1010;
    track.left[1019] &= 0x1801;
    track.right[1019] &= 0x1801;
    track.flags[1019] |= 0x2010;

    int r = rng.Next(140) + 220;
    if ((track.right[r] & 0x0200) != 0) {
        --r;
    }
    track.left[r] &= 0x1801;
    track.right[r] &= 0x1801;
    track.flags[r] |= 0x8000;

    r = rng.Next(600) + 390;
    if ((track.right[r] & 0x0200) != 0) {
        --r;
    }
    track.left[r] &= 0x1801;
    track.right[r] &= 0x1801;
    track.flags[r] |= 0x8000;

    r = rng.Next(600) + 390;
    if ((track.right[r] & 0x0200) != 0) {
        --r;
    }
    track.left[r] &= 0x1801;
    track.right[r] &= 0x1801;
    track.flags[r] |= 0x4000;

    // The cursor advances THROUGH each block.
    for (int i = 220; i + 90 < 380;) {
        for (int k = 0; k < 17; ++k) {
            track.elev[i++] = GENTLE[k];
        }
        i += 20;
        for (int k = 0; k < 17; ++k) {
            track.elev[i++] = static_cast<int16_t>(-GENTLE[k]);
        }
        i += 30;
    }
    track.flags[385] |= 0x0100;
}

// Both hazards are placed only past this segment, i.e.
constexpr int HAZARD_FROM   = 400;
constexpr int ROCK_ODDS     = 0x40;   // 1 in 64
constexpr int OIL_ODDS      = 0x50;   // 1 in 80
constexpr int ROCK_MIN_GAP  = 0x46;   // 70 segments
constexpr int HAZARD_LANES  = 0x10;
constexpr uint16_t FLAG_ROCK = 0x0004;
constexpr uint16_t FLAG_OIL  = 0x1000;
constexpr int PILE_FROM     = 0xf0;   // segment 240
constexpr int PILE_ODDS     = 180;    // roadType * 100 + 80

// Where the shoulder walls step, and by how much.
struct WallBlock { int from; int to; int leftStep; int rightStep; };
constexpr WallBlock WALL_BLOCKS[] = {
    {380, 398,  200, -200},
    {400, 418, -200,  200},
    {500, 518, -200,  200},
    {600, 618,  200, -200},
    {700, 718,  200, -200},
};
constexpr int WALL_FROM = 200;
constexpr int WALL_FLAT_BELOW = 380;      // the drag window has no walls
constexpr uint16_t RIGHT_DROP_BIT = 0x8000;

void BuildWalls(TrackData& track, int segments) {
    for (int s = 0; s < TRACK_SEGMENTS; ++s) {
        track.wallLeft[s] = 0;
        track.wallRight[s] = 0;
    }
    for (int s = WALL_FROM; s < segments; ++s) {
        if (s < WALL_FLAT_BELOW) continue;          // already zero
        // The original steps whole blocks of ten, chosen by the block's first segment.
        const int block = s - s % 10;
        int leftStep = 0, rightStep = 0;
        for (const WallBlock& b : WALL_BLOCKS) {
            if (block >= b.from && block <= b.to) {
                leftStep = b.leftStep;
                rightStep = b.rightStep;
                break;
            }
        }
        track.wallLeft[s] = static_cast<int16_t>(track.wallLeft[s - 1] + leftStep);
        track.wallRight[s] = static_cast<int16_t>(track.wallRight[s - 1] + rightStep);
        // A drop on the right is published to the object bits, which is what arms the 39 mph drop-off crash.
        if (track.wallRight[s] < 0) {
            track.right[s] |= RIGHT_DROP_BIT;
            track.right[s] &= static_cast<uint16_t>(~0x0001);
        }
    }
}

void ScatterHazards(TrackData& track, RaceRng& rng, TrackGenSource source) {
    if (source != TrackGenSource::StreetRod2) return;   // Street Rod has none
    const int segments = std::min(kStreetRod2Params.segments, TRACK_SEGMENTS);

    for (int s = 0; s < TRACK_SEGMENTS; ++s) track.lane[s] = 0;
    BuildWalls(track, segments);

    // The gap counter ticks once per segment and once more each time the roll comes up.
    int rockGap = 0;
    int pileGap = 0;
    for (int s = 0; s < segments; ++s) {
        if (s > HAZARD_FROM) {
            if (rng.Next(ROCK_ODDS) == 0 && rockGap++ > ROCK_MIN_GAP) {
                track.flags[s] |= FLAG_ROCK;
                track.lane[s] = static_cast<int16_t>(rng.Next(HAZARD_LANES) + 1);
                rockGap = 0;
            }
            // Oil takes whatever the rock left: never both on one segment.
            if (rng.Next(OIL_ODDS) == 0 && (track.flags[s] & FLAG_ROCK) == 0) {
                track.flags[s] |= FLAG_OIL;
                track.lane[s] = static_cast<int16_t>(rng.Next(HAZARD_LANES) + 1);
            }
        }
        // Rock piles reaching onto the road (object bit 0x10, its neighbour 0x1000), 70 apart, only below a wall.
        if (s > PILE_FROM && pileGap > ROCK_MIN_GAP && s + 1 < TRACK_SEGMENTS) {
            if (rng.Next(PILE_ODDS) == 0 && track.wallLeft[s + 1] > 0) {
                track.left[s] |= 0x0010;
                track.left[s - 1] |= 0x1000;
                pileGap = 0;
            }
            if (rng.Next(PILE_ODDS) == 0 && track.wallRight[s + 1] > 0) {
                track.right[s] |= 0x0010;
                track.right[s - 1] |= 0x1000;
                pileGap = 0;
            }
        }
        ++rockGap;
        ++pileGap;
    }
}

const TrackGenParams& TrackParams(TrackGenSource source) {
    return source == TrackGenSource::StreetRod2 ? kStreetRod2Params : kStreetRodParams;
}

bool LoadTrackTables(const std::vector<uint8_t>& exeData, TrackGenSource source) {
    const bool isSr2 = source == TrackGenSource::StreetRod2;
    const size_t at = isSr2 ? kStreetRod2VariantsAt : kStreetRodVariantsAt;
    const int width = isSr2 ? kStreetRod2Params.shapesPerRun : kStreetRodParams.shapesPerRun;
    if (exeData.size() < at + size_t(10 * width) * 2) return false;

    int rows[10][MAX_SHAPES_PER_RUN] = {};
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < width; ++c) {
            const size_t o = at + (size_t(r) * width + c) * 2;
            const int code = (int(exeData[o]) << 8) | int(exeData[o + 1]);
            // Every entry names one of the eleven shape codes.
            if (code < 1 || code > 11) return false;
            rows[r][c] = code;
        }
    }
    if (isSr2) {
        if (exeData.size() < kStreetRod2HillsAt + size_t(HILL_COUNT) * 2) return false;
        int16_t hills[HILL_COUNT];
        for (int i = 0; i < HILL_COUNT; ++i) {
            const size_t o = kStreetRod2HillsAt + size_t(i) * 2;
            hills[i] = static_cast<int16_t>((int(exeData[o]) << 8) | int(exeData[o + 1]));
            // Each entry nudges a block of ten by one or two units.
            if (hills[i] < -2 || hills[i] > 2) return false;
        }
        memcpy(HILL_PROFILE, hills, sizeof(HILL_PROFILE));
    }

    memcpy(SHAPE_SEQUENCES[isSr2 ? 1 : 0], rows, sizeof(rows));
    return true;
}

// The second elevation pass. Cumulative, so each block of ten shifts the road
// up or down and the terrain keeps rolling for the whole generated stretch.
void ApplyHillProfile(TrackData& track, int segments) {
    for (int block = 0; block < HILL_COUNT; ++block) {
        const int start = HILL_START + block * HILL_BLOCK;
        if (start + HILL_BLOCK > segments) break;
        const int delta = HILL_PROFILE[block];
        // The run-in to the finish stays flat whatever the table says.
        const bool runIn = start >= HILL_FLAT_FROM && start <= HILL_FLAT_TO;
        for (int i = 0; i < HILL_BLOCK; ++i) {
            const int seg = start + i;
            // A narrow crossing is flat too, so the road does not tilt under
            // the one place it is hardest to steer.
            const bool blocked = runIn || (track.flags[seg] & HILL_BLOCKED_FLAG) != 0;
            const int prev = seg > 0 ? track.elev[seg - 1] : 0;
            track.elev[seg] = static_cast<int16_t>(prev + (blocked ? 0 : delta));
        }
    }
}

void GenerateTrackShapes(TrackData& track, int variant, TrackGenSource source) {
    gShape = &TrackParams(source);
    gVariantRows = SHAPE_SEQUENCES[source == TrackGenSource::StreetRod2 ? 1 : 0];

    const int selectedVariant = ((variant % 10) + 10) % 10;
    const int segments = std::min(gShape->segments, TRACK_SEGMENTS);
    for (int i = 380; i < segments; ++i) {
        ClearGeneratedSegment(track, i, 1);
    }

    // FUN_0024c1d6 builds the walls first; its narrows only go where the walls differ.
    if (source == TrackGenSource::StreetRod2) {
        BuildWalls(track, segments);
    }

    GenState state;
    for (int i = 0; i < gShape->shapesPerRun; ++i) {
        RunShape(track, state, gVariantRows[selectedVariant][i]);
    }

    while (state.gen_pos < segments) {
        if (InLimit(state.gen_pos)) {
            track.curve[state.gen_pos] = static_cast<int16_t>(state.cur_curve);
            track.elev[state.gen_pos] = 0;
        }
        ++state.gen_pos;
    }

    if (gShape->rollingHills) {
        ApplyHillProfile(track, segments);
    }
}


// tshape.c's hazard records, as shipped; the documented fallback for
// LoadHazardShapes.
namespace {

struct HazardShape {
    uint16_t asset;                       // as stored, one based
    HazardRect phase[HAZARD_PHASES];
};

const HazardShape kHazardShapes[HazardKindCount] = {
    {284,
    {{  0,  0, 19,  9},
     {  0,  9, 12,  7},
     {  0, 16,  8,  5},
     {  8, 18,  7,  3},
     { 12,  9,  6,  2},
     { 13, 12,  4,  1},
     { 13, 14,  3,  1},
     { 16, 16,  3,  1},
     { 17, 18,  2,  1},
     { 18, 20,  1,  1},
     { 18, 20,  1,  1}}},
    {297,
    {{  0,  0, 40, 10},
     {  0, 10, 25,  6},
     {  0, 16, 16,  4},
     { 25, 10, 13,  3},
     { 25, 13,  9,  2},
     { 16, 16,  8,  2},
     { 17, 19,  6,  1},
     { 24, 19,  5,  1},
     { 30, 19,  4,  1},
     { 35, 19,  3,  1},
     { 36, 19,  2,  1}}},
    {285,
    {{  0,  0, 28,  9},
     {  0, 10, 17,  6},
     {  0, 17, 12,  4},
     { 18, 10,  9,  3},
     { 18, 14,  8,  3},
     { 20, 18,  7,  2},
     { 13, 18,  6,  2},
     { 13, 21,  5,  2},
     {  0, 22,  3,  1},
     {  5, 22,  3,  1},
     {  9, 22,  2,  1}}},
};

HazardShape gHazards[HazardKindCount];
bool gHazardsReady = false;

void EnsureHazardShapes() {
    if (gHazardsReady) return;
    std::memcpy(gHazards, kHazardShapes, sizeof(gHazards));
    gHazardsReady = true;
}

}  // namespace

bool LoadHazardShapes(const std::vector<uint8_t>& exeData) {
    const int records[HazardKindCount] = {HAZARD_ROCK_RECORD, HAZARD_OIL_RECORD, HAZARD_PILE_RECORD};
    HazardShape got[HazardKindCount];

    for (int k = 0; k < HazardKindCount; ++k) {
        const size_t at = HAZARD_SHAPE_TABLE_OFFSET +
                          size_t(records[k]) * HAZARD_RECORD_SIZE;
        if (exeData.size() < at + HAZARD_RECORD_SIZE) {
            EnsureHazardShapes();
            return false;
        }
        const uint8_t* rec = exeData.data() + at;
        got[k].asset = static_cast<uint16_t>((rec[0] << 8) | rec[1]);
        // The record has to be the one we mean, or the sprite is wrong.
        if (got[k].asset != kHazardShapes[k].asset) {
            EnsureHazardShapes();
            return false;
        }
        for (int j = 0; j < HAZARD_PHASES; ++j) {
            const uint8_t* q = rec + 10 + j * 4;
            got[k].phase[j] = HazardRect{q[0], q[1], q[2], q[3]};
            // Phase 0 is the nearest and every later one is smaller; a phase
            // with no area would blit nothing.
            if (q[2] == 0 || q[3] == 0) {
                EnsureHazardShapes();
                return false;
            }
        }
    }

    std::memcpy(gHazards, got, sizeof(gHazards));
    gHazardsReady = true;
    return true;
}

int HazardAsset(int kind) {
    EnsureHazardShapes();
    if (kind < 0 || kind >= HazardKindCount) return 0;
    return gHazards[kind].asset - 1;
}

const HazardRect& HazardRectFor(int kind, int phase) {
    EnsureHazardShapes();
    static const HazardRect kNone;
    if (kind < 0 || kind >= HazardKindCount) return kNone;
    if (phase < 0 || phase >= HAZARD_PHASES) return kNone;
    return gHazards[kind].phase[phase];
}

}
