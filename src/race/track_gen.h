#pragma once
// Track generation port of road_layout_init (0x239f78) and
// TrackGenerator (0x239e2e). See RodLegacyResearch/docs/driving/track-data.md.

#include "race/race_types.h"

namespace srrace {

// Builds the full 1120-segment track: city rows 0..179, countryside
// 180..1119, finish gantries at 379/1019, random arches and S-curves.
// roadType is the original A4-0x3fc4 value (default 1).
void RoadLayoutInit(TrackData& track, RaceRng& rng, int roadType = 1);

// Regenerates segments 380..1119 (preserving object bits 0x1801) from the
// 12-shape sequence for variant 0..9, writing curve/elev/flags.
void GenerateTrackShapes(TrackData& track, int variant);

}  // namespace srrace
