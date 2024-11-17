#pragma once
#include <string>
#include "parsers/libp.h"

using namespace std;

struct CarModel {
    // For custom cars
    string id;
    string filename;

    // Currently not known where this is defined
    int baseSpeed;
    
    // From SR_data
    int year;
    string name;
    CarVendor_t vendor;
    BitmapOffset_t offsets;
    
    // From lib.p
    int width, height;    
    int index;
};

bool operator<(const CarModel &a, const CarModel &b);