#pragma once

// Debug-UI glue for main.cpp

#include <SDL2/SDL.h>

#include <vector>

#include "debug_ui.h"

// Gathers the debug panel contents for one frame
std::vector<DebugPanel> BuildDebugPanels(Uint64 frameDelta);

// Maps a mouse event from window pixels into game pixels
void RemapMouseToGameViewport(SDL_Event& event, const SDL_Rect& vp);
