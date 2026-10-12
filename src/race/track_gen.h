/*
 * RodLegacy - Track generation: RoadLayoutInit and GenerateTrackShapes build the 1120-segment road.
 *
 * Created: 2026-07-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "race/race_types.h"
#include "race/aqueduct.h"

#include <cstdint>
#include <vector>

namespace srrace {

// Which game's generator to run.
enum class TrackGenSource { StreetRod, StreetRod2 };

struct TrackGenParams {
    int segments;         // how much track there is: 1120 / 990
    int genLimit;         // where the shapes stop writing: 0x442 / 0x3de
    int shapesPerRun;     // shapes taken from the variant row: 12 / 13
    int curveStep;        // the step a curve shape walks per segment: 3 / 7
    uint16_t objectMask;  // what a shape keeps when clearing: 0x1801 / 0x9811
    // Street Rod 2 collapsed eleven shape functions to seven.
    bool hillsAreCurves;
    // Street Rod 2 runs a second pass after the shapes, accumulating elevation from a 61-entry table one delta
    // per block of ten segments.
    bool rollingHills;
};

const TrackGenParams& TrackParams(TrackGenSource source);

// Fills the shoulder walls and scatters Street Rod 2's two on-road hazards over the Mulholland window.
void ScatterHazards(TrackData& track, RaceRng& rng, TrackGenSource source);

// Replaces the per-variant shape table.
bool LoadTrackTables(const std::vector<uint8_t>& exeData, TrackGenSource source);

// Lays out the town and the countryside.
constexpr int GRUDGE_SEGMENTS = 117;
constexpr int GRUDGE_ROAD_FIRST = 191;
constexpr int GRUDGE_FINISH_MARKER = 88;
constexpr uint16_t GRUDGE_DASH_BIT = 0x0008;
constexpr uint16_t GRUDGE_DASH_COLOUR_BIT = 0x0010;

void BuildGrudgeStrip(TrackData& track);

// Street Rod 2's two on-road hazards past segment 400. tshape.c holds 23 records of 54 bytes at 0x25ccac,
// each one lib.p asset in eleven distance phases. Asset numbers are stored one based, as in aqushape.
//
//   flags & 0x0004   entry 7,  lib.p 283 - the rock: stops you dead over 30 mph
//   flags & 0x1000   entry 22, lib.p 296 - the oil:  slides you over 50 mph
constexpr int HAZARD_PHASES = 11;
constexpr uint16_t HAZARD_ROCK_FLAG = 0x0004;
constexpr uint16_t HAZARD_OIL_FLAG = 0x1000;
// tshape.c's table, which is Street Rod's roadside_sprite_table with three records appended.
constexpr size_t HAZARD_SHAPE_TABLE_OFFSET = 0x39ebc;   // in the SR2 executable
constexpr int HAZARD_RECORD_SIZE = 54;
constexpr int HAZARD_ROCK_RECORD = 7;
constexpr int HAZARD_OIL_RECORD = 22;
// Entry 8, lib.p 284: with entry 7 it makes the rock pile jdraw.c FUN_0023ca96 draws for an object's 0x10 bit.
constexpr int HAZARD_PILE_RECORD = 8;
constexpr int ROADSIDE_RECORDS_SR1 = 20;
constexpr int ROADSIDE_RECORDS_SR2 = 23;

enum HazardKind { HazardRock = 0, HazardOil = 1, HazardPile = 2, HazardKindCount = 3 };

struct HazardRect {
    uint8_t srcX = 0, srcY = 0, w = 0, h = 0;
};

// Zero-based lib.p id, i.e. the stored number less one.
int HazardAsset(int kind);
const HazardRect& HazardRectFor(int kind, int phase);

// Replaces the built-in copy with the table in the player's own executable.
bool LoadHazardShapes(const std::vector<uint8_t>& exeData);

void RoadLayoutInit(TrackData& track, RaceRng& rng, int roadType = 1,
                    TrackGenSource source = TrackGenSource::StreetRod);

// Regenerates segments 380 onwards from the shape sequence for variant 0..9, writing curve/elev/flags and
// preserving the object bits the source's mask names.
void GenerateTrackShapes(TrackData& track, int variant,
                         TrackGenSource source = TrackGenSource::StreetRod);

}
