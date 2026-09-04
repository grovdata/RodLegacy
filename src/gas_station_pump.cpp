#include "gas_station_pump.h"
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
