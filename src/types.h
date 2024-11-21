#pragma once
#include <string>
#include "parsers/libp.h"
#include <SDL.h>

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

    void Log() const {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, 
            "CarModel: id=%s, filename=%s, baseSpeed=%d, year=%d, name=%s, vendor=%d, width=%d, height=%d, index=%d",
            id.c_str(), filename.c_str(), baseSpeed, year, name.c_str(), vendor, width, height, index);
    }
};

bool operator<(const CarModel &a, const CarModel &b);