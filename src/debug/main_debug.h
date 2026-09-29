#pragma once

#include <SDL2/SDL.h>

#include <vector>

#include "debug/debug_ui.h"

std::vector<DebugPanel> BuildDebugPanels(Uint64 frameDelta);

void RemapMouseToGameViewport(SDL_Event& event, const SDL_Rect& vp);
