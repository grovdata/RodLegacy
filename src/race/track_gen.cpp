#include "race/track_gen.h"

#include <algorithm>

namespace srrace {
namespace {

// NOTE on curve[] vs elev[]: in this engine curve[] (0x1098) renders as left/right
// curves and drives the lateral drift; elev[] (0x7d8) renders as up/down hills.
// The generator's shapes are named for that rendered effect:
//   - the "ramp" shapes (ShapeCurveLeft/Right, ShapeCurveHump/Dip) write curve[]
//     via the cur_curve accumulator -> they produce CURVES. ShapeCurveLeft sets
//     flag 0x0200 (draws the turn-left sign, slot 16); ShapeCurveRight sets 0x0400
//     (turn-right sign, slot 17) - these are the left/right CURVE markers (the
//     original mislabelled them uphill/downhill markers).
//   - the "table" shapes (ShapeHillShallow/Deep) copy the +-5 SHALLOW/DEEP tables
//     into elev[] -> they produce HILLS.
// (The original binary's arrays/shapes are labelled the opposite way round; the
// names here follow what you actually see on screen.)

constexpr int GEN_START = 390;
constexpr int GEN_LIMIT = 0x442;

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

constexpr int SHAPE_SEQUENCES[10][12] = {
    {2, 3, 1, 7, 4, 10, 5, 8, 6, 2, 9, 1},
    {11, 4, 8, 1, 6, 9, 2, 5, 7, 2, 3, 1},
    {1, 5, 1, 3, 8, 11, 6, 7, 2, 4, 9, 1},
    {3, 1, 4, 9, 2, 6, 8, 1, 3, 8, 10, 2},
    {10, 9, 5, 1, 6, 2, 3, 7, 4, 2, 8, 2},
    {2, 3, 8, 10, 5, 11, 4, 7, 1, 6, 9, 2},
    {1, 5, 9, 1, 4, 2, 3, 10, 8, 4, 1, 9},
    {2, 4, 7, 11, 2, 5, 8, 4, 1, 7, 6, 2},
    {2, 6, 8, 10, 6, 1, 3, 7, 5, 1, 4, 9},
    {1, 3, 1, 6, 1, 4, 9, 10, 6, 8, 2, 3},
};

struct GenState {
    int gen_pos = GEN_START;
    int cur_curve = 0;
};

bool InLimit(int pos) {
    return pos >= 0 && pos < GEN_LIMIT;
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

// TrackGenerator's re-clear of segments 380..1119 (0x239e2e). The three masks
// are exact: `andi.w #0xf117 / #0x87ff / #0xe7ff` at 0x239e66/0x239e70/0x239e7a.
// This is NOT the 0x1801 clear the shape functions use - it deliberately keeps
// what road_layout_init already placed here:
//   flags  &= 0xf117 -> keeps the overhead arch/gantry nibble (0xf000), sprite
//                       slot 5 (0x100), the line-colour bit (0x10) and city /
//                       sign / side-street (0x7); drops width, dash and the
//                       curve/narrow marker bits that the shapes re-place.
//   right  &= 0x87ff / left &= 0xe7ff -> keeps trees, poles, billboards and
//                       houses; drops only the narrow-warning and barrier bits
//                       (and the right-side special-building bits).
// Zeroing flags here wiped the "County line" finish gantry at 1019 and both
// random arches in 390..989, so a road race showed two arches instead of five
// and no banner at the finish; the 0x1801 object mask also stripped the
// scenery from the whole generated two thirds of the course.
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
        track.left[pos] &= 0x1801;
        track.right[pos] &= 0x1801;
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

    int k = curCurve / 3;
    const int end = k + 40;
    while (k < end && state.gen_pos < GEN_LIMIT) {
        track.flags[state.gen_pos] |= 0x0008;
        track.curve[state.gen_pos] = static_cast<int16_t>(k * 3);
        ++state.gen_pos;
        ++k;
    }
    state.cur_curve = k * 3;
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

    int k = curCurve / 3;
    const int end = k - 45;
    while (k > end && state.gen_pos < GEN_LIMIT) {
        track.flags[state.gen_pos] |= 0x0008;
        track.curve[state.gen_pos] = static_cast<int16_t>(k * 3);
        ++state.gen_pos;
        --k;
    }
    state.cur_curve = k * 3;
}

void CopyHillTable(TrackData& track, GenState& state, const int16_t* table, int count, bool negate) {
    int idx = 0;
    while (idx < count && state.gen_pos < GEN_LIMIT) {
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
        if (state.gen_pos >= GEN_LIMIT) {
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
    switch (shapeId) {
    case 1: ShapeCurveLeft(track, state); break;
    case 2: ShapeCurveRight(track, state); break;
    case 3: ShapeHillShallow(track, state, false); break;
    case 4: ShapeHillShallow(track, state, true); break;
    case 5: ShapeHillDeep(track, state, false); break;
    case 6: ShapeHillDeep(track, state, true); break;
    case 7: ShapeNarrowRight(track, state); break;
    case 8: ShapeNarrowLeft(track, state); break;
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

}  // namespace

void RoadLayoutInit(TrackData& track, RaceRng& rng, int roadType) {
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

    // Two overhead arches somewhere along each of the three cruises, one with
    // each banner. The original does these with BSET #6 / #7 on byte +0 of the
    // word (road_layout_init, IRA LAB_0D5C), which is the HIGH byte - so bits
    // 14 and 15, not 6 and 7. Ported as 0x0040/0x0080 they landed on roadside
    // object bits instead and the town had no arches at all.
    track.flags[10 + rng.Next(45)] |= 0x4000;
    track.flags[10 + rng.Next(45)] |= 0x8000;
    track.flags[65 + rng.Next(55)] |= 0x4000;
    track.flags[65 + rng.Next(55)] |= 0x8000;
    track.flags[115 + rng.Next(55)] |= 0x4000;
    track.flags[115 + rng.Next(55)] |= 0x8000;

    track.right[44] |= 0x0001;
    track.left[29] |= 0x1000;
    track.left[24] |= 0x0800;
    track.left[108] |= 0x0001;
    track.left[87] |= 0x1000;
    track.left[82] |= 0x0800;
    track.left[156] |= 0x0001;
    track.left[135] |= 0x1000;
    track.left[130] |= 0x0800;

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

    for (int i = 220; i + 90 < 380;) {
        for (int k = 0; k < 17; ++k) {
            track.elev[i + k] = GENTLE[k];
        }
        i += 20;
        for (int k = 0; k < 17; ++k) {
            track.elev[i + k] = static_cast<int16_t>(-GENTLE[k]);
        }
        i += 30;
    }
    track.flags[385] |= 0x0100;
}

void GenerateTrackShapes(TrackData& track, int variant) {
    const int selectedVariant = ((variant % 10) + 10) % 10;
    for (int i = 380; i < TRACK_SEGMENTS; ++i) {
        ClearGeneratedSegment(track, i, 1);
    }

    GenState state;
    for (int i = 0; i < 12; ++i) {
        RunShape(track, state, SHAPE_SEQUENCES[selectedVariant][i]);
    }

    while (state.gen_pos < TRACK_SEGMENTS) {
        if (InLimit(state.gen_pos)) {
            track.curve[state.gen_pos] = static_cast<int16_t>(state.cur_curve);
            track.elev[state.gen_pos] = 0;
        }
        ++state.gen_pos;
    }
}

}  // namespace srrace
