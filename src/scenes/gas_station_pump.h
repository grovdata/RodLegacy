/*
 * RodLegacy - Gas pump arithmetic, SDL-free: ApplyGasPumpTick.
 *
 * Created: 2026-09-04
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

struct GasPumpTick {
    int fuelTenths;
    int bankroll;
    bool dispensed;  // true if fuel/money actually changed this tick
    bool done;        // tank full or out of money - stop calling
};

GasPumpTick ApplyGasPumpTick(int fuelTenths, int bankroll, int pricePerTick,
                              int fuelPerTickTenths, int tankCapTenths);
