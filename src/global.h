#pragma once

#define GAME_WIDTH 320
#define GAME_HEIGHT 200
#define GAME_RACING_HEIGHT 120
#define GAME_STATUSBAR_HEIGHT 9
#define GAME_FPS 60
#define LIBP_FILE "../../streetrod-bins/amiga/SR_disk2/lib.p"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_timer.h>
#include <math.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "types.h"
#include "driver.h"
#include "SFont.h"
#include "utils.h"

extern SDL_Renderer *renderer;
extern SFont_Font *font;
