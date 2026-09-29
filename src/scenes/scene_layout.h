/*
 * RodLegacy - Loads a scene's clickable zones from embedded TOML.
 *
 * Created: 2026-04-24
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "ui/zone.h"
#include <vector>

// onClick must be wired by the caller.
class SceneLayout {
public:
    static std::vector<Zone> Load(const char* tomlText);
};
