#pragma once
#include <string>

enum CarVendor {
    CHRYSLER = 0,
    FORD = 1,
    GM = 2,
    CUSTOM = 3
};

struct CarModel {
    std::string id;
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
    int index;
};

bool operator<(const CarModel &a, const CarModel &b);