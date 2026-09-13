#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <SDL.h>

namespace fs = std::filesystem;

// Describes one expected zip pack.
struct ZipPack {
    std::string filename;                  // e.g. "sr1amiga.zip"
    bool mandatory;
    std::string description;               // shown on screen
    std::vector<std::string> adfContents;  // ADF filenames expected inside the zip
};

// Result of scanning a directory for one pack.
struct ZipPackStatus {
    ZipPack pack;
    bool zipFound;  // zip file exists in scanned directory
    bool zipValid;  // zip exists AND contains all adfContents entries
};

// Canonical list of all expected packs.
const std::vector<ZipPack>& GetZipPacks();

// Scan a directory and return status for each pack.
std::vector<ZipPackStatus> ScanDirectory(const fs::path& dir);

// Config/setup screen shown when game files are missing.
// Runs its own SDL event loop. Returns chosen game directory,
// or empty path if the user quit.
fs::path RunConfigScreen(SDL_Renderer* renderer, SDL_Joystick* joystick);
