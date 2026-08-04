#pragma once
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Pure binary parser for the Amiga Street Rod .sav format. Format reference:
// RodLegacyResearch/docs/formats/save_game.md -- every offset/field here
// matches that doc exactly. This file has no State dependency; State wires
// a LoadedSave into itself separately (see State::LoadSaveGame).
//
// Known gaps carried over from the format doc, not resolved here:
//  - Segment 4 (spare parts) is a flat list with no per-entry category byte,
//    unlike installed parts (whose category comes from car sub-list slot
//    position) -- spareParts below cannot be sorted by category from the
//    raw bytes alone.
//  - Part variant index -> named part type is only confirmed for a handful
//    of values (see save_game.md); everything else is an opaque index here.
//  - The King (opponent slot 21) has no known car-spec source distinct from
//    the fixed constant already used elsewhere; not populated here.
//  - Header offset 0x2C counter and trailer chunk 8 are parsed but unused.

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

    // Slot 0=Transmission, 1=Engine, 2=Manifold, 3=Carb, 4/5=unmapped, 6=Tires.
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

    int racesEntered = 0;
    int racesWon = 0;

    // Trailer chunk 6: 22 records, position i = driver slot i (0-20 = pool
    // drivers, 21 = The King -- see save_game.md; inferred from matching
    // count + King driving his own signature car, not independently
    // confirmed in-game). Value is the car spec that driver currently drives.
    int opponentCarSpec[22] = {0};

    uint32_t elapsedTicks = 0;  // trailer chunk 7, raw
};

struct SaveLoadError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SaveGameLoader {
public:
    // Throws SaveLoadError on malformed or too-small input.
    static LoadedSave Load(const std::filesystem::path& path);
};
