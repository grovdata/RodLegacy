/*
 * RodLegacy - Street Rod 2's engine bay geometry, from amparts.c's position tables (FUN_0023678a and the
 * functions it calls). Positions are absolute screen pixels, top-left of the sprite.
 *
 * Created: 2026-10-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL.h>
#include <vector>

constexpr SDL_Point kSr2BayOrigin = {8, 13};
constexpr int kSr2BayWidth = 304;
constexpr int kSr2BayHeight = 137;
// Parts are clipped at this screen row (uRam00266f60 = 0x7c, relative to the bay's top).
constexpr int kSr2BayClipBottom = 124;

struct Sr2BayPart {
    int asset = -1;          // lib.p index
    SDL_Point at = {0, 0};   // top-left
    bool present = false;
};

struct Sr2BayScrew {
    SDL_Point at;            // left, bottom: the crop shrinks upwards from here
};

struct Sr2BayLayout {
    Sr2BayPart engine;
    Sr2BayPart manifold;
    Sr2BayPart carbs[3];
    Sr2BayPart airFilters[3];
    // Engine and manifold screws first (4, or 6 for a supercharger), then two per carburettor.
    std::vector<Sr2BayScrew> bodyScrews;
    std::vector<Sr2BayScrew> carbScrews[3];
    SDL_Point wire = {65, 107};     // plug, 24x17
    SDL_Point looseWire = {126, 111};  // 56x13, redrawn over parts that cover it
};

// engineClass 0 V6, 1 small V8, 2 big V8. manifoldClass 0..8 (2brl, 4brl, 3-2brl, 2-4brl, racing, tunnel ram,
// dual tunnel ram, injection, supercharger). carbClass 0 2bbl, 1 4bbl, 2 racing 4bbl. carbCount is how many of the
// manifold's carburettors are fitted.
Sr2BayLayout ComputeSr2BayLayout(int engineClass, int manifoldClass, int carbClass, int carbCount);

int Sr2EngineAsset(int engineClass);
int Sr2ManifoldAsset(int manifoldClass);
int Sr2CarbAsset(int carbClass);
// Height of the 8x6 screw sprite crop for a screw state (0 seated .. 3).
int Sr2ScrewCropHeight(int state);
