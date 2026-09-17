#include "doctest/doctest.h"
#include "core/zone_nav.h"

using nav::Dir;
using nav::Rect;

// A 2x2 grid of buttons, the shape most dialogs and menus have.
static const std::vector<Rect> kGrid = {
    {10, 10, 40, 20},   // 0: top-left,     centre (30, 20)
    {110, 10, 40, 20},  // 1: top-right,    centre (130, 20)
    {10, 60, 40, 20},   // 2: bottom-left,  centre (30, 70)
    {110, 60, 40, 20},  // 3: bottom-right, centre (130, 70)
};

TEST_CASE("PickDirectional moves one step in each direction") {
    // Standing on the top-left button.
    CHECK(nav::PickDirectional(kGrid, 30, 20, Dir::Right) == 1);
    CHECK(nav::PickDirectional(kGrid, 30, 20, Dir::Down) == 2);
    // Nothing above or to the left of it.
    CHECK(nav::PickDirectional(kGrid, 30, 20, Dir::Up) == -1);
    CHECK(nav::PickDirectional(kGrid, 30, 20, Dir::Left) == -1);

    // And back from the bottom-right.
    CHECK(nav::PickDirectional(kGrid, 130, 70, Dir::Up) == 1);
    CHECK(nav::PickDirectional(kGrid, 130, 70, Dir::Left) == 2);
}

TEST_CASE("PickDirectional prefers the straight neighbour over a diagonal") {
    // The diagonal is closer as the crow flies, but straight down is the move.
    const std::vector<Rect> zones = {
        {100, 30, 10, 10},  // 0: down and well off to the side, centre (105, 35)
        {0, 60, 10, 10},    // 1: straight down, centre (5, 65)
    };
    CHECK(nav::PickDirectional(zones, 5, 5, Dir::Down) == 1);
}

TEST_CASE("PickDirectional ignores the target under the cursor") {
    // Regression: the old code only considered zones containing the cursor, so
    // navigation could never leave the one it was already on.
    const std::vector<Rect> zones = {
        {0, 0, 100, 100},  // 0: the cursor is inside this one
        {0, 200, 20, 20},  // 1: the only real target below
    };
    CHECK(nav::PickDirectional(zones, 50, 50, Dir::Down) == 1);
}

TEST_CASE("PickDirectional needs strict movement, not a tie") {
    // Same centre line: neither is "to the right" of the other.
    const std::vector<Rect> zones = {{0, 0, 10, 10}, {0, 40, 10, 10}};
    CHECK(nav::PickDirectional(zones, 5, 5, Dir::Right) == -1);
    CHECK(nav::PickDirectional(zones, 5, 5, Dir::Down) == 1);
}

TEST_CASE("PickNearest finds the closest centre, cursor inside or not") {
    CHECK(nav::PickNearest(kGrid, 0, 0) == 0);
    CHECK(nav::PickNearest(kGrid, 200, 200) == 3);
    CHECK(nav::PickNearest(kGrid, 125, 15) == 1);
}

TEST_CASE("empty target lists report no pick") {
    const std::vector<Rect> none;
    CHECK(nav::PickNearest(none, 0, 0) == -1);
    CHECK(nav::PickDirectional(none, 0, 0, Dir::Up) == -1);
}
