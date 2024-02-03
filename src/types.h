#pragma once
#include <string>

enum CarVendor { CHRYSLER, FORD, GM };

struct CarModel {
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