#pragma once

#include <SDL.h>
#include "car/car_system.h"
#include "parsers/libp.h"

// Absolute 320x200 screen space. The tables store a bottom-left anchor, so pass the drawn bitmap's rect;
// only its w/h are read.
SDL_Rect EngineBodyRect(SDL_Rect asset, CarEngine_t engine);
SDL_Rect ManifoldBodyRect(SDL_Rect asset, ManifoldType manifold);

// Two 88px-wide bodies: the triple 2-barrel always, and the 2-barrel on a V6 block.
bool ManifoldBodyIsNarrow(ManifoldType manifold, CarEngine_t engine);
SDL_Rect EngineWireRect(SDL_Rect asset, CarEngine_t engine);

SDL_Rect CarbBodyRect(SDL_Rect asset, ManifoldType manifold, CarEngine_t engine, int carbIndex);

// Five drawn parts: 0=engine, 1=manifold, 2..4=carburetor 0..2 - one per
// pointer on the original's car struct.
constexpr int kEngineBayPartSlots = 2 + kMaxCarburetors;

// 0=engine, 1=manifold-left, 2=manifold-right, 3..5=carb 0..2. x is the sprite's left edge, y its bottom,
// so a screw whose visible crop shrinks stays seated.
constexpr int kEngineBayScrewCount = 6;
SDL_Point EngineBayScrewAnchor(int screwIndex, CarEngine_t engine, ManifoldType manifold,
                               CarburetorType carb);
SDL_Rect EngineBayScrewZone(SDL_Point anchor);
