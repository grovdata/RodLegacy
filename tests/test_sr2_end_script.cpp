#include <doctest/doctest.h>

#include "scenes/sr2_end_script.h"

namespace {
std::vector<uint8_t> Blob(const std::vector<uint8_t>& frames) {
    std::vector<uint8_t> d(504, 0);
    d[0] = 24; d[1] = 55; d[2] = 0x01; d[3] = 0x08; d[4] = 33;   // rect 0: 24x55 at (264,33)
    d[6] = 32; d[7] = 13;                                        // rect 1
    d.insert(d.end(), frames.begin(), frames.end());
    return d;
}
}

TEST_CASE("SR2_end frames end at the first entry with a hold") {
    Sr2EndScript s;
    REQUIRE(Sr2EndParse(Blob({1, 111, 150, 0, 0, 5, 6, 3, 1, 2, 7, 2, 255, 0, 0, 0}), s));
    REQUIRE(s.rects.size() == 84);
    CHECK(s.rects[0].x == 264);
    CHECK(s.rects[0].y == 33);
    REQUIRE(s.frames.size() == 2);
    CHECK(s.frames[0].sprites.size() == 2);
    CHECK(s.frames[0].hold == 3);
    CHECK(s.frames[1].sprites.size() == 1);
    CHECK(s.frames[1].hold == 2);
}

TEST_CASE("SR2_end rejects data that does not parse") {
    Sr2EndScript s;
    CHECK_FALSE(Sr2EndParse({1, 2, 3}, s));
    CHECK_FALSE(Sr2EndParse(Blob({1, 0, 0, 0, 255, 0, 0, 0}), s));   // frame never closed
    CHECK_FALSE(Sr2EndParse(Blob({200, 0, 0, 1, 255, 0, 0, 0}), s)); // rect index out of range
    CHECK_FALSE(Sr2EndParse(Blob({1, 0, 0, 1}), s));                 // no terminator
}
