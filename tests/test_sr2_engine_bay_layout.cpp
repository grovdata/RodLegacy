#include <doctest/doctest.h>

#include "car/sr2_engine_bay_layout.h"

// Every number is from a live capture of the original's blits: V6, 2-barrel manifold, 2-barrel carburettor.
TEST_CASE("SR2 bay layout matches the original's draws for a V6 with a 2-barrel setup") {
    Sr2BayLayout l = ComputeSr2BayLayout(0, 0, 0, 1);
    CHECK(l.engine.asset == 8);
    CHECK(l.engine.at.x == 79);
    CHECK(l.engine.at.y == 68);
    CHECK(l.manifold.asset == 12);
    CHECK(l.manifold.at.x == 124);
    CHECK(l.manifold.at.y == 75);
    REQUIRE(l.bodyScrews.size() == 4);
    CHECK(l.bodyScrews[0].at.x == 135);
    CHECK(l.bodyScrews[0].at.y == 89);
    CHECK(l.bodyScrews[1].at.x == 182);
    CHECK(l.bodyScrews[2].at.x == 130);
    CHECK(l.bodyScrews[2].at.y == 103);
    CHECK(l.bodyScrews[3].at.x == 186);
    CHECK(l.carbs[0].present);
    CHECK(l.carbs[0].asset == 14);
    CHECK(l.carbs[0].at.x == 100);
    CHECK(l.carbs[0].at.y == 61);
    REQUIRE(l.carbScrews[0].size() == 2);
    CHECK(l.carbScrews[0][0].at.x == 143);
    CHECK(l.carbScrews[0][0].at.y == 101);
    CHECK(l.carbScrews[0][1].at.x == 170);
    CHECK(l.airFilters[0].present);
    CHECK(l.airFilters[0].at.x == 144);
    CHECK(l.airFilters[0].at.y == 65);
}

TEST_CASE("SR2 bay layout: a V8 sits lower than a V6, injection has no carburettors") {
    Sr2BayLayout v8 = ComputeSr2BayLayout(1, 1, 1, 1);
    CHECK(v8.engine.at.y == 58);
    CHECK(v8.engine.asset == 9);
    Sr2BayLayout fi = ComputeSr2BayLayout(2, 7, 1, 0);
    CHECK_FALSE(fi.carbs[0].present);
    CHECK(fi.engine.asset == 21);
    CHECK(fi.manifold.asset == 30);
    Sr2BayLayout sc = ComputeSr2BayLayout(2, 8, 1, 0);
    CHECK(sc.bodyScrews.size() == 6);
}
