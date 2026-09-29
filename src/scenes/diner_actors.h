/*
 * RodLegacy - The diner's waitress and phone guy animation tables, from the original.
 *
 * Created: 2026-09-07
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <cstdint>

// Both actors are one-shot sequences, not loops.
namespace srdiner {

// 84 steps, 1-based frames into an 11-frame sheet. Ends on frame 11, the standing pose.
inline constexpr uint8_t kWaitressFrames[84] = {
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 9, 10, 10, 1, 1, 2, 2, 3, 3, 4,
    4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 11
};
inline constexpr int kWaitressSteps = 84;
inline constexpr int kWaitressFrameCount = 11;
inline constexpr int kWaitressColumns = 6;

// She decelerates into her stop on the way in and accelerates away on the way out.
struct WaitressRun { int count; int dx; };
inline constexpr WaitressRun kWaitressRuns[2][3] = {
    {{78, 3}, {3, 2}, {3, 1}},     // dir 0: in from -40, stops at 203
    {{4, -1}, {3, -2}, {77, -3}},  // dir 1: out from 200, leaves at -41
};
inline constexpr int kWaitressStartX[2] = {-40, 200};

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

// Deviation, with extra_animations on: she stops once on the way in. 3 px steps, so the stop is the nearest step.
// The still pose is the last frame of the other sheet (lib.p 335); 336's frame 11 is mid-stride.
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

// 44 steps over 20 frames, 0-9 on the first sheet and 10-19 on the second, 5x2 each.
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

// The rows differ in height and the sprite is bottom-aligned, so its top edge moves with the row.
inline constexpr int kGuyX = 187;
inline constexpr int kGuyBottomY = 124;
inline constexpr int kGuyFrameW = 40;
inline constexpr int kGuyRowH[2] = {60, 64};

inline constexpr int kWaitressY = 68;

}
