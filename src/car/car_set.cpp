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
    // Registry order is the merged table order and is written into saves, so it must stay stable.
    static const std::vector<CarSetDef> defs = [] {
        std::vector<CarSetDef> d;

        CarSetDef amiga{"amiga", "Street Rod", CarSetArt::AmigaLibP,
                        "", "", "", ""};
        amiga.providesDrivers = true;          // Mike, Vinnie, The King
        amiga.game = SrGame::SR1;
        d.push_back(amiga);

        // The data disk adds cars only. It ships no driver portraits, so
        // switching it on never changes who you race against.
        CarSetDef srse{"srse", "Street Rod SE", CarSetArt::DosLib,
                       "srsepc.zip", "SRSE/LIB2", "SRSE/HOT_DATA", "SRSE"};
        srse.providesDrivers = false;
        d.push_back(srse);

        // Street Rod 2's own cars and drivers, read from its lib.p and
        // SR2_data exactly as the stock set reads Street Rod's.
        CarSetDef sr2{"sr2amiga", "Street Rod 2", CarSetArt::AmigaLibP,
                      "", "", "", ""};
        sr2.providesDrivers = true;            // Phil, Betty, The King
        sr2.game = SrGame::SR2;
        d.push_back(sr2);

        return d;
    }();
    return defs;
}

int CarSetForGame(SrGame game) {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    for (size_t i = 0; i < defs.size(); i++) {
        if (defs[i].zipName.empty() && defs[i].game == game) return static_cast<int>(i);
    }
    return kAmigaCarSet;
}
