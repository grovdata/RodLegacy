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

// The Amiga stock set is mandatory and first, keeping its car-spec indices at 0..25 as original .SAV
// files store them. Optional sets append in registry order.

enum class CarSetArt {
    AmigaLibP,  // recoloured through the car palette
    DosLib,     // fixed 16-colour DOS art that carries its own paint
};

struct CarSetDef {
    std::string id;        // stable key, safe to write into a save
    std::string name;      // shown to the player
    bool        mandatory; // the stock set cannot be switched off
    CarSetArt   art;

    // Relative to the game data directory. Empty for the Amiga set, which uses lib.p/SR_data.
    std::string zipName;    // e.g. "srsepc.zip"
    std::string libEntry;   // path inside the zip, e.g. "SRSE/LIB2"
    std::string dataEntry;  // path inside the zip, e.g. "SRSE/HOT_DATA"
    std::string looseDir;   // directory name when unzipped beside the game data
};

// Registry order is the merged car-spec table order: keep it stable, append new sets.
const std::vector<CarSetDef>& GetCarSetDefs();

static constexpr int kAmigaCarSet = 0;

// Not in any car data file: the wheel cell is bottom-aligned to the body sprite, and every wheel arch is
// cut for that Y.
int CarSetWheelY(int carSet, int bodySpriteHeight);

// -1 means the car has no such variant.
struct CarSprites {
    int body = -1;
    int choppedRoof = -1;
    int airScoop = -1;
    int rearBumperStripped = -1;
    int frontBumperStripped = -1;
};
