#pragma once
#include <string>
#include "parsers/libp.h"


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
    int hoodScoopX, hoodScoopY;
    int index;
};

bool operator<(const CarModel &a, const CarModel &b);