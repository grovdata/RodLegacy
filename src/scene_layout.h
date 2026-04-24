#pragma once

#include "ui/zone.h"
#include <vector>

// Loads scene zones from embedded TOML text (null-terminated).
// Returns zones with id, text, and rect set; onClick must be wired by the caller.
class SceneLayout {
public:
    static std::vector<Zone> Load(const char* tomlText);
};
