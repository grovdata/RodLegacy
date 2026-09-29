#pragma once

struct GasPumpTick {
    int fuelTenths;
    int bankroll;
    bool dispensed;  // true if fuel/money actually changed this tick
    bool done;        // tank full or out of money - stop calling
};

GasPumpTick ApplyGasPumpTick(int fuelTenths, int bankroll, int pricePerTick,
                              int fuelPerTickTenths, int tankCapTenths);
