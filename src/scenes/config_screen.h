#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <SDL.h>

namespace fs = std::filesystem;

struct ZipPack {
    std::string filename;                  // e.g. "sr1amiga.zip"
    bool mandatory;
    std::string description;
    std::vector<std::string> adfContents;
};

struct ZipPackStatus {
    ZipPack pack;
    bool zipFound;
    bool zipValid;
};

const std::vector<ZipPack>& GetZipPacks();

std::vector<ZipPackStatus> ScanDirectory(const fs::path& dir);

// Config/setup screen shown when game files are missing.
// Runs its own SDL event loop. Returns chosen game directory,
// or empty path if the user quit.
fs::path RunConfigScreen(SDL_Renderer* renderer);
