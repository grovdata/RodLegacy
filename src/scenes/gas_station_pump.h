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

// Street Rod 2: $1 a tick, a whole delivery each time, and the pump stops once the next one would not fit.
GasPumpTick ApplySr2GasPumpTick(int fuel, int bankroll, int perTick, int tankCap);

// SR2 lets a fill overshoot to 200 but drops the tank to 170 (GAS_MAX_AMOUNT) on leaving.
int Sr2FuelOnLeaving(int fuel);
