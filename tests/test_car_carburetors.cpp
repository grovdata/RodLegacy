#include <doctest/doctest.h>
#include "car_system.h"

// The original carries three separate carburetor pointers on its car struct
// and the manifold class decides how many are used - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md. Car mirrors that, so
// pulling one carb off a dual quad leaves the other fitted and the car dead.
//
// Car itself pulls in SDL and the whole asset stack, so this pins the shared
// data the model is built on rather than constructing a Car.

TEST_SUITE("Carburetors per manifold") {

TEST_CASE("manifold class order is the binary's, not ManifoldType's") {
    CHECK(ManifoldClass(MANIFOLD_2BARREL) == 0);
    CHECK(ManifoldClass(MANIFOLD_4BARREL) == 1);
    // The two that don't line up: the original has triple at 2, dual quad 3.
    CHECK(ManifoldClass(MANIFOLD_TRIPLE_2BARREL) == 2);
    CHECK(ManifoldClass(MANIFOLD_DUAL_QUAD) == 3);
    CHECK(ManifoldClass(MANIFOLD_RACING) == 4);
}

TEST_CASE("carb count matches the A4-0x3b9c table 1,1,3,2,2") {
    CHECK(CarburetorsPerManifold(MANIFOLD_2BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_4BARREL) == 1);
    CHECK(CarburetorsPerManifold(MANIFOLD_TRIPLE_2BARREL) == 3);
    CHECK(CarburetorsPerManifold(MANIFOLD_DUAL_QUAD) == 2);
    // Racing runs two, which is why it needs two carbs out of spares.
    CHECK(CarburetorsPerManifold(MANIFOLD_RACING) == 2);
}

TEST_CASE("every manifold fits within the three mounts a car has") {
    const ManifoldType all[] = {MANIFOLD_2BARREL, MANIFOLD_4BARREL, MANIFOLD_DUAL_QUAD,
                                MANIFOLD_TRIPLE_2BARREL, MANIFOLD_RACING};
    for (ManifoldType m : all) {
        CHECK(CarburetorsPerManifold(m) >= 1);
        CHECK(CarburetorsPerManifold(m) <= kMaxCarburetors);
    }
}

}  // TEST_SUITE
