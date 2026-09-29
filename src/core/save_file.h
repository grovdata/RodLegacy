#pragma once
#include "core/save_loader.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Only what cannot be recomputed is stored; asset ids, power figures and names come back from CarBuilder on load.

namespace fs = std::filesystem;

// Throws SaveLoadError if the file is unreadable.
struct SaveSummary {
    std::string playerName;
    int bankroll = 0;
    uint32_t elapsedTicks = 0;
};
SaveSummary PeekSave(const fs::path& path);

std::vector<fs::path> ListSaveFiles(const fs::path& dir);

// Wired to --selftest-save: catches Car changes that silently stop round-tripping.
bool RunSaveRoundTripSelfTest();
