#pragma once
#include "core/save_loader.h"  // SaveLoadError

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// RodLegacy's own save format: a plain TOML document holding the game objects
// directly (Car, Part, EnginePart, Reputation, RacerEntry). Written to
// AssetManager::GetSavesDir().
//
// The reader and writer are State::LoadGameToml / State::SaveGameToml,
// declared in state.h but defined in save_file.cpp so all the TOML lives in
// one place. There is deliberately no intermediate record type: unlike the
// Amiga .sav format (save_loader.h), whose opaque variant bytes and pointer
// garbage need real translation, this format's fields are the game's own.
//
// Only what cannot be recomputed is stored. A part keeps its type enum, wear,
// cost, vendor and classified-ad name; its asset id, horsepower, torque and
// generated name come back from CarBuilder::Create*Part / EnginePart on load.

namespace fs = std::filesystem;

// Enough of a save's header to label a row in the load/save list without
// disturbing the running game. Throws SaveLoadError if the file is unreadable.
struct SaveSummary {
    std::string playerName;
    int bankroll = 0;
    uint32_t elapsedTicks = 0;
};
SaveSummary PeekSave(const fs::path& path);

// Every *.toml in dir, sorted by filename. Empty if dir does not exist.
std::vector<fs::path> ListSaveFiles(const fs::path& dir);

// Saves the running game to a scratch file, reloads it, and checks the state
// that came back matches what went in. Returns true on success and logs the
// first mismatch otherwise. Wired to --selftest-save: this format serialises
// live game objects, so a change to Car or its parts can silently stop
// round-tripping, and there is no way to catch that without a real State.
bool RunSaveRoundTripSelfTest();
