#include "doctest/doctest.h"
#include "race/sr2_gauge.h"

namespace {

// An executable with the dial table and streams at the documented offsets; every dial gets the same stream.
std::vector<uint8_t> MakeExe(int len, std::vector<uint8_t> stream) {
    std::vector<uint8_t> exe(0x40000, 0);
    for (int i = 0; i < srrace::SR2_DIAL_COUNT; ++i) {
        uint8_t* r = exe.data() + 0x3f5a0 + i * 14;
        auto put16 = [&](int at, int v) { r[at] = (v >> 8) & 0xff; r[at + 1] = v & 0xff; };
        put16(0, 100 + i);   // x0
        put16(2, 50);        // y0
        put16(4, -10);       // dx
        put16(6, 5);         // dy
        put16(8, len);
        r[13] = 0;           // stream at the base
    }
    for (size_t k = 0; k < stream.size(); ++k) exe[0x14f24 + k] = stream[k];
    return exe;
}

}  // namespace

TEST_CASE("an SR2 needle walks its chain code from the start offset") {
    // op 2 (+1,+1) for 4 steps, then op 3 (-1,0) for 4 steps: len 8.
    srrace::Sr2Gauges g;
    REQUIRE(g.Load(MakeExe(8, {static_cast<uint8_t>((2 << 5) | 3), static_cast<uint8_t>((3 << 5) | 3)})));

    // value 0: no steps, the tip is the start offset (x0 + dx, y0 - dy)
    srrace::Sr2Needle n = g.Tip(0, 0);
    CHECK(n.x0 == 100);
    CHECK(n.y0 == 50);
    CHECK(n.x1 == 90);
    CHECK(n.y1 == 45);

    // value 65 of 130 is half the stream: 4 steps of (+1,+1)
    n = g.Tip(0, 65);
    CHECK(n.x1 == 94);
    CHECK(n.y1 == 49);

    // full scale stops one step short of the end
    n = g.Tip(0, 130);
    CHECK(n.x1 == 94 - 3);
    CHECK(n.y1 == 49);

    // a value past the scale clamps
    CHECK(g.Tip(0, 999).x1 == n.x1);
}

TEST_CASE("each dial has its own pivot") {
    srrace::Sr2Gauges g;
    REQUIRE(g.Load(MakeExe(2, {static_cast<uint8_t>((1 << 5) | 1)})));
    CHECK(g.Tip(0, 0).x0 == 100);
    CHECK(g.Tip(5, 0).x0 == 105);
}

TEST_CASE("an SR2 executable whose streams do not add up is refused") {
    srrace::Sr2Gauges g;
    // runs of 6 and 6 overshoot a length of 8
    CHECK_FALSE(g.Load(MakeExe(8, {static_cast<uint8_t>(5), static_cast<uint8_t>(5)})));
    CHECK_FALSE(g.Ready());
    CHECK_FALSE(g.Load(std::vector<uint8_t>(100, 0)));
}
