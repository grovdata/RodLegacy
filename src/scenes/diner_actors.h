#pragma once
#include <cstdint>

// The diner's two actors, ported from the original's tables. Both are
// one-shot sequences, not free-running loops - see
// RodLegacyResearch/docs/diner.md.
namespace srdiner {

// Waitress: 84 steps, frame numbers 1-based into an 11-frame sheet. Three
// turns of a 24-step walk cycle, then a short one ending on frame 11, the
// standing pose she holds once she arrives.
inline constexpr uint8_t kWaitressFrames[84] = {
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 11
};
inline constexpr int kWaitressSteps = 84;
inline constexpr int kWaitressFrameCount = 11;
inline constexpr int kWaitressColumns = 6;

// X as runs of a constant step, so she decelerates into her stop on the way
// in and accelerates away on the way out. Three runs each, 84 steps total.
struct WaitressRun { int count; int dx; };
inline constexpr WaitressRun kWaitressRuns[2][3] = {
    {{78, 3}, {3, 2}, {3, 1}},     // dir 0: in from -40, stops at 203
    {{4, -1}, {3, -2}, {77, -3}},  // dir 1: out from 200, leaves at -41
};
inline constexpr int kWaitressStartX[2] = {-40, 200};

// Where she is after `step` steps of the walk, 0 being her starting point.
inline constexpr int WaitressX(int dir, int step) {
    int x = kWaitressStartX[dir];
    for (const WaitressRun& run : kWaitressRuns[dir]) {
        const int n = step < run.count ? step : run.count;
        x += n * run.dx;
        step -= n;
        if (step <= 0) break;
    }
    return x;
}

// Not the original's: with extra_animations on she stops once on the way in,
// standing still. She moves 3 px a step, so the stop lands on the nearest step
// rather than exactly on kWaitressPauseX (129 falls between 128 and 131).
//
// The still pose is the last frame of lib.p 335, the left-hand sheet: 336's
// frame 11 is mid-stride with an arm out, 335's is her stood upright with her
// feet together. So the stop borrows the other sheet while she is not moving.
// (The 6x2 grid's twelfth cell is not a spare pose - it is blank on both.)
inline constexpr int kWaitressPauseX = 129;
inline constexpr float kWaitressPauseMs = 3000.0f;
inline constexpr uint8_t kWaitressStandFrame = 11;
inline constexpr int kWaitressStandSheet = 1;  // sheet order is {336, 335}

inline constexpr int WaitressPauseStep(int targetX) {
    int best = 0;
    int bestErr = 1 << 30;
    for (int s = 0; s < kWaitressSteps; ++s) {
        int err = WaitressX(0, s) - targetX;
        if (err < 0) err = -err;
        if (err < bestErr) { bestErr = err; best = s; }
    }
    return best;
}

// Phone guy: 44 steps over 20 frames, 0-9 on the first sheet and 10-19 on
// the second, five columns by two rows each.
inline constexpr uint8_t kGuyFrames[44] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 6,
    9, 10, 11, 12, 13, 14, 15, 16, 17, 16, 15, 14, 13, 12,
    10, 11, 12, 13, 14, 15, 18, 19, 18, 15, 14, 13, 12,
    10, 11, 12, 13, 14, 15, 0
};
inline constexpr int kGuySteps = 44;
// Hold, in animation steps, looked up by frame number rather than by step.
inline constexpr uint8_t kGuyHold[20] = {
    2, 2, 2, 4, 2, 2, 2, 4, 2, 2, 6, 2, 2, 4, 2, 8, 2, 4, 2, 6
};

// The two rows are different heights and the sprite is bottom-aligned, so
// its top edge moves with the row.
inline constexpr int kGuyX = 187;
inline constexpr int kGuyBottomY = 124;
inline constexpr int kGuyFrameW = 40;
inline constexpr int kGuyRowH[2] = {60, 64};

inline constexpr int kWaitressY = 68;

}  // namespace srdiner
