#pragma once

#include "race/race_types.h"

namespace srrace {

// Full 1120-segment track: city 0..179, countryside 180..1119, finish gantries at 379/1019.
void RoadLayoutInit(TrackData& track, RaceRng& rng, int roadType = 1);

// Regenerates segments 380..1119 from the variant's 12-shape sequence, preserving object bits 0x1801.
void GenerateTrackShapes(TrackData& track, int variant);

}
