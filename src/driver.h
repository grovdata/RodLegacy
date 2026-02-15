#pragma once

#include <iostream>
#include <string>
#include "types.h"

struct Driver {
    std::string name;
    int preferredCar;
    int spriteIndex;
    int stringOffsetName;
    std::string customDriverPath;
};

//const Driver drivers[] = {{"Foo", 0, 0, 0}};