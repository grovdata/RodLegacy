#pragma once

// Pure gas-pump dispensing math, kept SDL-free so it is unit testable (see
// tests/test_gas_station_pump.cpp). Pacing verified against the Amiga
// disassembly - see RodLegacyResearch/docs/driving/gas_station.md.
struct GasPumpTick {
    int fuelTenths;
    int bankroll;
    bool dispensed;  // true if fuel/money actually changed this tick
    bool done;        // tank full or out of money - stop calling
};

// Applies one dispensing tick: adds fuelPerTickTenths (clamped to
// tankCapTenths) and charges min(pricePerTick, bankroll), never taking
// bankroll below zero. Does nothing if the tank is already full or bankroll
// is already zero.
GasPumpTick ApplyGasPumpTick(int fuelTenths, int bankroll, int pricePerTick,
                              int fuelPerTickTenths, int tankCapTenths);
