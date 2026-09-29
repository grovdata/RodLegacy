/*
 * RodLegacy - Game-wide constants (screen size, frame rate) and the pause-aware GameTicks clock.
 *
 * Created: 2023-02-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#define GAME_WIDTH 320
#define GAME_HEIGHT 200
#define GAME_RACING_HEIGHT 120
#define GAME_RACING_HORIZON 60
#define GAME_STATUSBAR_HEIGHT 10
#define GAME_FPS 60

#include <SDL.h>
#include <SDL_messagebox.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_timer.h>
#include <math.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <map>

#include "core/types.h"
#include "race/driver.h"
#include "gfx/SFont.h"
#include "core/utils.h"

extern SDL_Window *win;
extern SDL_Renderer *renderer;
extern bool isFullScreen;

// Milliseconds since start excluding pauses; scene timing uses this so a pause freezes it.
Uint64 GameTicks();
