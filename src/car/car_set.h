/*
 * RodLegacy - Car sets: the Amiga stock set and optional DOS data-disk packs, their sprite layout and wheel
 * line.
 *
 * Created: 2026-08-08
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <string>
#include <vector>

#include "parsers/libp.h"   // SrGame


enum class CarSetArt {
    AmigaLibP,  // recoloured through the car palette
    DosLib,     // fixed 16-colour DOS art that carries its own paint
};

struct CarSetDef {
    std::string id;        // stable key, safe to write into a save
    std::string name;      // shown to the player
    CarSetArt   art;

    // Relative to the game data directory. Empty for the Amiga set, which uses lib.p/SR_data.
    std::string zipName;    // e.g. "srsepc.zip"
    std::string libEntry;   // path inside the zip, e.g. "SRSE/LIB2"
    std::string dataEntry;  // path inside the zip, e.g. "SRSE/HOT_DATA"
    std::string looseDir;   // directory name when unzipped beside the game data

    // A set brings cars, drivers, or both.
    bool providesCars    = true;
    bool providesDrivers = false;

    // Which game this set's content belongs to.
    SrGame game = SrGame::SR1;
};

// Registry order is the merged car-spec table order: keep it stable, append new sets.
const std::vector<CarSetDef>& GetCarSetDefs();

static constexpr int kAmigaCarSet = 0;

// True for the set that reads `game`'s own lib.p and data file.
inline bool CarSetIsOwnGame(const CarSetDef& def, SrGame game) {
    return def.zipName.empty() && def.game == game;
}

// Whether a set's cars can race in a given game.
inline bool CarSetFitsGame(const CarSetDef& def, SrGame game) {
    return static_cast<int>(def.game) <= static_cast<int>(game);
}

// The set that reads a given game's own lib.p and data file.
int CarSetForGame(SrGame game);

int CarSetWheelY(int carSet, int bodySpriteHeight);

// -1 means the car has no such variant.
struct CarSprites {
    int dashboard = 0;
    int bodyStyle = 0;
    int body = -1;
    int choppedRoof = -1;
    int airScoop = -1;
    int rearBumperStripped = -1;
    int frontBumperStripped = -1;
};
