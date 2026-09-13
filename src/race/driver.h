#pragma once

#include <iostream>
#include <string>
#include "core/types.h"

struct Driver {
    std::string name;
    int preferredCar;
    int spriteIndex;
    int stringOffsetName;
    // Row this driver came from in SR_data's driver block, or -1 when the
    // table was unavailable and the fallback list was used.
    int srDriverIndex = -1;
    std::string customDriverPath;
};

//const Driver drivers[] = {{"Foo", 0, 0, 0}};