#include "doctest/doctest.h"
#include "part.h"

// SellSparePartsLoop (Amiga 0x0024284a); see docs/gameplay.md "Selling".
TEST_CASE("PartSalePrice: engines and gearboxes are priced on wear") {
    // 90% of value at zero wear, then rounded down to a multiple of 5.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 0, 0) == 450);
    CHECK(PartSalePrice(PartCategory::Transmission, 500, 0, 19) == 450);
    // Half worn, half the money.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 50, 0) == 225);
    // Worn out is worthless.
    CHECK(PartSalePrice(PartCategory::Engine, 500, 100, 0) == 0);
    // Rounding is down, never up: 100*100*9/1000 = 90.
    CHECK(PartSalePrice(PartCategory::Engine, 103, 0, 0) == 90);
}

TEST_CASE("PartSalePrice: carbs and manifolds are priced on the roll, not wear") {
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 0, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Carburetor, 100, 90, 0) == 70);
    CHECK(PartSalePrice(PartCategory::Manifold, 100, 0, 19) == 85);
}

TEST_CASE("PartSalePrice: tires always fetch nothing") {
    CHECK(PartSalePrice(PartCategory::Tire, 150, 0, 19) == 0);
}
