#pragma once

#include <SDL.h>
#include "car_system.h"
#include "parsers/libp.h"

// Engine bay overview geometry, read out of the Amiga binary's own position
// tables rather than eyeballed - see
// RodLegacyResearch/docs/garage/engine_bay_layout.md for the addresses and
// the pixel-exact verification. Everything below is absolute 320x200 screen
// space.
//
// The tables store a bottom-left anchor, so a part's top edge depends on
// which asset is actually loaded: pass the drawn bitmap's own rect
// (Bitmap::GetPosition()); only its w/h are read.
SDL_Rect EngineBodyRect(SDL_Rect asset, CarEngine_t engine);
SDL_Rect ManifoldBodyRect(SDL_Rect asset, ManifoldType manifold);
SDL_Rect EngineWireRect(SDL_Rect asset, CarEngine_t engine);

// How many carburetors the manifold class mounts: 1, 2 or 3. They all use
// the same asset, each in its own slot.
int CarbCount(ManifoldType manifold);
SDL_Rect CarbBodyRect(SDL_Rect asset, ManifoldType manifold, CarEngine_t engine, int carbIndex);

// Six screws: 0=engine, 1=manifold-left, 2=manifold-right, 3..5=carb 0..2.
// Returns the mount point: x is the sprite's left edge, y its bottom edge,
// so a screw whose visible crop shrinks stays seated at the same spot.
constexpr int kEngineBayScrewCount = 6;
SDL_Point EngineBayScrewAnchor(int screwIndex, CarEngine_t engine, ManifoldType manifold,
                               CarburetorType carb);
SDL_Rect EngineBayScrewZone(SDL_Point anchor);
