#pragma once

#define GAME_WIDTH 320
#define GAME_HEIGHT 200
#define GAME_STATUSBAR_HEIGHT 9

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
