/*
 * RodLegacy - Debug UI glue for main.cpp: the session panels and mouse remapping into the game viewport.
 *
 * Created: 2026-08-27
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL.h>

#include <vector>

#include "debug/debug_ui.h"

std::vector<DebugPanel> BuildDebugPanels(Uint64 frameDelta);

void RemapMouseToGameViewport(SDL_Event& event, const SDL_Rect& vp);
