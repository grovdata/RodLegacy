/*
 * RodLegacy - A driver: name and portrait.
 *
 * Created: 2023-01-29
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <iostream>
#include <string>
#include "core/types.h"

struct Driver {
    std::string name;
    int preferredCar;
    // Portrait asset, relative to driverSet's lib rather than to any one loaded lib.p.
    int spriteIndex;
    int stringOffsetName;
    // Row in SR_data's driver block, -1 when the fallback list was used.
    int srDriverIndex = -1;
    // Which set in GetCarSetDefs() this driver and their portrait came from.
    int driverSet = 0;
    std::string customDriverPath;
};
