#pragma once
#include "parsers/libp.h"


struct Part {
    int wear;
    int cost;
    CarVendor_t vendor;
    AmigaLibAsset assetId;
};