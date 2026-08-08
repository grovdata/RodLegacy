#include "car_set.h"

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
