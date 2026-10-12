/*
 * RodLegacy - The setup screen shown when game files are missing: finds and checks the data zips.
 *
 * Created: 2026-04-14
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <SDL.h>

#include "parsers/libp.h"

namespace fs = std::filesystem;

struct ZipPack {
    std::string filename;                  // e.g. "sr1amiga.zip"
    bool mandatory;                        // one mandatory pack is enough to play
    std::vector<std::string> adfContents;
};

struct ZipPackStatus {
    ZipPack pack;
    bool zipFound;
    bool zipValid;
};

// Canonical list of all expected packs. The game packs are taken from the
// game table so the screen cannot name a disk the loader does not look for.
inline const std::vector<ZipPack>& GetZipPacks() {
    static const std::vector<ZipPack> packs = [] {
        std::vector<ZipPack> v;
        for (SrGame g : {SrGame::SR1, SrGame::SR2}) {
            const SrGameFiles& gf = SrGameGetFiles(g);
            v.push_back({gf.zipName, true,
                         {SrLibArchiveNames[gf.disk1], SrLibArchiveNames[gf.disk2]}});
        }
        v.push_back({"srsepc.zip", false, {}});
        return v;
    }();
    return packs;
}

// Either game is enough to start, so a mandatory pack is one of a set.
inline bool AnyMandatoryValid(const std::vector<ZipPackStatus>& status) {
    for (const ZipPackStatus& s : status) {
        if (s.pack.mandatory && s.zipValid) return true;
    }
    return false;
}

std::vector<ZipPackStatus> ScanDirectory(const fs::path& dir);

// Config/setup screen shown when game files are missing.
// Runs its own SDL event loop. Returns chosen game directory,
// or empty path if the user quit.
fs::path RunConfigScreen(SDL_Renderer* renderer);

// Street Rod 1 or 2, for a folder holding both. False if the user quit.
bool RunGamePicker(SDL_Renderer* renderer, SrGame* game);
