#pragma once
#include <string>

enum CarVendor {
    CHRYSLER = 0,
    FORD = 1,
    GM = 2,
    CUSTOM = 3
};

enum CarModelId {
    CAR_MODEL_GM_CHEVROLET_2DR,
    CAR_MODEL_MOD_SAAB_92
};

struct CarModel {
    CarModelId id;
    CarVendor vendor;
    std::string name;
    std::string filename;
    int year;
    int baseSpeed;
    int width, height;
    int driverX, driverY;
    int rearWheelX, rearWheelY;
    int frontWheelX, frontWheelY;
    int gasCapX, gasCapY;
};

bool operator<(const CarModel &a, const CarModel &b);