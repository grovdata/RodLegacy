#include <doctest/doctest.h>

#include "scenes/gas_station_pump.h"

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

TEST_CASE("the SR2 pump adds a whole delivery for one dollar") {
    GasPumpTick tick = ApplySr2GasPumpTick(/*fuel=*/34, /*bankroll=*/200, /*perTick=*/40, /*cap=*/200);
    CHECK(tick.dispensed);
    CHECK(tick.fuelTenths == 74);
    CHECK(tick.bankroll == 199);
    CHECK_FALSE(tick.done);
}

TEST_CASE("the SR2 pump stops when the next delivery would not fit") {
    // 34 + 4 * 40 = 194; 200 - 194 = 6 is less than 40.
    int fuel = 34;
    int bank = 200;
    int ticks = 0;
    for (GasPumpTick t; ticks < 20; ) {
        t = ApplySr2GasPumpTick(fuel, bank, 40, 200);
        fuel = t.fuelTenths;
        bank = t.bankroll;
        ++ticks;
        if (t.done) break;
    }
    CHECK(ticks == 4);
    CHECK(fuel == 194);
    CHECK(bank == 196);   // four ticks, four dollars, as measured in the original
}

TEST_CASE("the SR2 regular pump delivers 30 and a pump with no room or no money does nothing") {
    CHECK(ApplySr2GasPumpTick(0, 100, 30, 200).fuelTenths == 30);
    GasPumpTick full = ApplySr2GasPumpTick(180, 100, 40, 200);
    CHECK_FALSE(full.dispensed);
    CHECK(full.done);
    GasPumpTick broke = ApplySr2GasPumpTick(0, 0, 40, 200);
    CHECK_FALSE(broke.dispensed);
    CHECK(broke.done);
}

TEST_CASE("Sr2FuelOnLeaving caps the tank at 170") {
    CHECK(Sr2FuelOnLeaving(200) == 170);
    CHECK(Sr2FuelOnLeaving(170) == 170);
    CHECK(Sr2FuelOnLeaving(54) == 54);
}
