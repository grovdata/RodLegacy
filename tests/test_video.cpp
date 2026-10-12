#include <doctest/doctest.h>
#include "core/video.h"

TEST_CASE("only whole window scales that fit the display are offered") {
    CHECK(video::MaxScaleFor(1440, 875, 28) == 4);    // MacBook Air: 1280x800 plus the title bar fits
    CHECK(video::MaxScaleFor(1920, 1040, 32) == 5);   // 1080p with a taskbar
    CHECK(video::MaxScaleFor(1280, 760, 30) == 3);    // 1280x800 with a taskbar: 4x is 30 px too tall
    CHECK(video::MaxScaleFor(3840, 2120, 32) == 10);  // 4K
    CHECK(video::MaxScaleFor(300, 180, 32) == 1);     // never below 1x
}
