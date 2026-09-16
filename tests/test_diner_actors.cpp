#include "doctest/doctest.h"
#include "scenes/diner_actors.h"

using namespace srdiner;

// Tables ported from the Amiga; see docs/diner.md.
TEST_CASE("waitress: the X runs and the frame table are the same length") {
    for (int dir = 0; dir < 2; ++dir) {
        int steps = 0;
        for (const WaitressRun& run : kWaitressRuns[dir]) steps += run.count;
        CHECK(steps == kWaitressSteps);
    }
}

TEST_CASE("waitress: walks in decelerating, walks out accelerating") {
    // In: -40 to 203, and the last steps are the small ones.
    CHECK(WaitressX(0, 0) == -40);
    CHECK(WaitressX(0, kWaitressSteps) == 203);
    CHECK(WaitressX(0, 1) - WaitressX(0, 0) == 3);
    CHECK(WaitressX(0, kWaitressSteps) - WaitressX(0, kWaitressSteps - 1) == 1);

    // Out: 200 to -41, and the first steps are the small ones.
    CHECK(WaitressX(1, 0) == 200);
    CHECK(WaitressX(1, kWaitressSteps) == -41);
    CHECK(WaitressX(1, 1) - WaitressX(1, 0) == -1);
    CHECK(WaitressX(1, kWaitressSteps) - WaitressX(1, kWaitressSteps - 1) == -3);
}

TEST_CASE("waitress: every step names a frame the sheet has") {
    for (uint8_t f : kWaitressFrames) {
        CHECK(f >= 1);
        CHECK(f <= kWaitressFrameCount);
    }
    // She ends standing, on the frame the 6x2 grid's twelfth cell is not.
    CHECK(kWaitressFrames[kWaitressSteps - 1] == 11);
}

TEST_CASE("waitress: the extra_animations stop lands on the walk's own grid") {
    // She moves 3 px a step, so 129 is not on it: 128 and 131 are.
    const int step = WaitressPauseStep(kWaitressPauseX);
    CHECK(WaitressX(0, step) == 128);
    CHECK(step > 0);
    CHECK(step < kWaitressSteps);
    // The pose she stands in is one the sheet actually has, and it comes off
    // 335 - the sheet she is not walking in on the way in.
    CHECK(kWaitressStandFrame <= kWaitressFrameCount);
    CHECK(kWaitressStandSheet == 1);
}

TEST_CASE("phone guy: frames stay inside the two sheets, holds cover them all") {
    for (uint8_t f : kGuyFrames) CHECK(f < 20);
    CHECK(kGuyFrames[0] == 0);
    CHECK(kGuyFrames[kGuySteps - 1] == 0);  // ends where it started
    for (uint8_t h : kGuyHold) CHECK(h > 0);
}
