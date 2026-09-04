#include <doctest/doctest.h>

#include "gas_station_pump.h"

TEST_CASE("ApplyGasPumpTick adds fuel and deducts bankroll") {
    GasPumpTick tick = ApplyGasPumpTick(/*fuel=*/0, /*bankroll=*/100, /*price=*/1,
                                         /*fuelPerTick=*/30, /*cap=*/170);
    CHECK(tick.dispensed);
    CHECK(tick.fuelTenths == 30);
    CHECK(tick.bankroll == 99);
    CHECK_FALSE(tick.done);
}

TEST_CASE("ApplyGasPumpTick clamps fuel to the tank cap") {
    GasPumpTick tick = ApplyGasPumpTick(160, 100, 1, 30, 170);
    CHECK(tick.fuelTenths == 170);
    CHECK(tick.done);
}

TEST_CASE("ApplyGasPumpTick takes only the remaining bankroll when it can't cover the price") {
    GasPumpTick tick = ApplyGasPumpTick(0, /*bankroll=*/1, /*price=*/2, 30, 170);
    CHECK(tick.dispensed);
    CHECK(tick.fuelTenths == 30);
    CHECK(tick.bankroll == 0);
    CHECK(tick.done);
}

TEST_CASE("ApplyGasPumpTick does nothing once bankroll is already zero") {
    GasPumpTick tick = ApplyGasPumpTick(50, 0, 1, 30, 170);
    CHECK_FALSE(tick.dispensed);
    CHECK(tick.fuelTenths == 50);
    CHECK(tick.bankroll == 0);
    CHECK(tick.done);
}

TEST_CASE("ApplyGasPumpTick does nothing once the tank is already full") {
    GasPumpTick tick = ApplyGasPumpTick(170, 100, 1, 30, 170);
    CHECK_FALSE(tick.dispensed);
    CHECK(tick.bankroll == 100);
    CHECK(tick.done);
}
