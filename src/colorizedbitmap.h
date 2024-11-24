#pragma once

#include "parsers/carcolor.h"
#include "parsers/libp.h"
#include "bitmap.h"
    
class ColorizedBitmap : public Bitmap {
private:
    StandardCarColor currentPalette;
    Asset asset;
    std::shared_ptr<Bitmap> baseBitmap;  // Keep reference to original for recoloring

public:
    ColorizedBitmap(std::shared_ptr<Bitmap> base, Asset asset) 
        : baseBitmap(base), asset(asset), currentPalette(CarColorRed) {}
    
    void ApplyPalette(StandardCarColor palette);
    Asset GetAsset() const { return asset; }
};