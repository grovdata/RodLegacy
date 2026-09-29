#include "car/car_set.h"

#include <algorithm>

namespace {

// lib.p packs 32x31 tire frames in one sheet; DOS libs use 40x28 sprites.
int TireHeight(CarSetArt art) {
    return art == CarSetArt::AmigaLibP ? 31 : 28;
}

}

int CarSetWheelY(int carSet, int bodySpriteHeight) {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    if (carSet < 0 || carSet >= (int)defs.size()) return 0;
    return std::max(0, bodySpriteHeight - TireHeight(defs[carSet].art));
}

const std::vector<CarSetDef>& GetCarSetDefs() {
    static const std::vector<CarSetDef> defs = {
        {
            "amiga", "Street Rod (stock)", true, CarSetArt::AmigaLibP,
            "", "", "", ""
        },
        {
            "srse", "Street Rod SE", false, CarSetArt::DosLib,
            "srsepc.zip", "SRSE/LIB2", "SRSE/HOT_DATA", "SRSE"
        },
    };
    return defs;
}
