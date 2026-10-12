#include "scenes/gas_station_pump.h"
#include <algorithm>

GasPumpTick ApplyGasPumpTick(int fuelTenths, int bankroll, int pricePerTick,
                              int fuelPerTickTenths, int tankCapTenths) {
    GasPumpTick result{fuelTenths, bankroll, false, false};
    if (fuelTenths < tankCapTenths && bankroll > 0) {
        result.fuelTenths = std::min(tankCapTenths, fuelTenths + fuelPerTickTenths);
        int charge = std::min(pricePerTick, bankroll);
        result.bankroll = bankroll - charge;
        result.dispensed = true;
    }
    result.done = result.fuelTenths >= tankCapTenths || result.bankroll <= 0;
    return result;
}

GasPumpTick ApplySr2GasPumpTick(int fuel, int bankroll, int perTick, int tankCap) {
    GasPumpTick result{fuel, bankroll, false, false};
    if (tankCap - fuel >= perTick && bankroll >= 1) {
        result.fuelTenths = fuel + perTick;
        result.bankroll = bankroll - 1;
        result.dispensed = true;
    }
    result.done = tankCap - result.fuelTenths < perTick || result.bankroll < 1;
    return result;
}

int Sr2FuelOnLeaving(int fuel) {
    return fuel < 170 ? fuel : 170;
}
