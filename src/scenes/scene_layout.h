#pragma once

#include "ui/zone.h"
#include <vector>

// onClick must be wired by the caller.
class SceneLayout {
public:
    static std::vector<Zone> Load(const char* tomlText);
};
