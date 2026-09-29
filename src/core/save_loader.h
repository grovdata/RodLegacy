/*
 * RodLegacy - Reader for original Amiga .sav files: SaveGameLoader parses them into LoadedSave records.
 *
 * Created: 2026-08-04
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Parser for the Amiga .sav format; no State dependency.
//
// Known gaps:
//  - Spare parts have no per-entry category byte, so they cannot be sorted by category.
//  - Part variant -> named type is only confirmed for a few values.
//  - The King (opponent slot 21) is not populated.
//  - Header offset 0x2C counter is parsed but unused.

struct LoadedPartRecord {
    int value = 0;    // dollars
    int wearRaw = 0;  // raw b2-b3; wear% = wearRaw/100, sentinel 0xFF80 = untracked
    int variant = 0;  // opaque sub-type index within its category
};

struct LoadedCarRecord {
    int value = 0;
    int specIndex = 0;
    int colorIndex = 0;
    uint8_t tuningScrews[6] = {3, 3, 3, 3, 3, 3};
    uint8_t wireState = 1;    // 0x01=plugged, 0xFF=unplugged
    int8_t tuningKnob = 0;    // signed, -8..+4, 0=perfectly tuned
    uint8_t fuelTenths = 0;   // gallons * 10
    uint8_t bodyByte = 0;     // low nibble: bumper/roof flags; high nibble: sticker index

    // Slot 0=Transmission, 1=Engine, 2=Manifold, 3..5=Carb, 6=Tires.
    std::vector<LoadedPartRecord> sublists[7];
};

struct LoadedSave {
    std::string name;
    int bankroll = 0;
    int ownedCarCount = 0;  // header's own count; gates roster size below
    bool hasActiveCar = false;
    LoadedCarRecord activeCar;
    std::vector<LoadedCarRecord> roster;       // ownedCarCount - 1 entries
    std::vector<LoadedPartRecord> spareParts;  // flat, category unknown per-entry

    // There is no races-won field: the original tracks standing, not wins.
    int racesCompleted = 0;
    int roadRep = 0;
    int dragRep = 0;
    bool lastRaceWon = false;

    // Index = driver slot (0-20 pool, 21 The King, inferred). Value is the car spec they drive.
    int opponentCarSpec[22] = {0};

    uint32_t elapsedTicks = 0;  // trailer chunk 7, raw
    int trackVariant = 0;       // trailer chunk 8: road-course template 0-9
};

struct SaveLoadError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SaveGameLoader {
public:
    // Throws SaveLoadError on malformed or too-small input.
    static LoadedSave Load(const std::filesystem::path& path);

    // For save bytes that are not a plain file, e.g. from an ADF image.
    static LoadedSave Parse(const std::vector<uint8_t>& data);
};
