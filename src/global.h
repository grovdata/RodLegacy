#pragma once

#define GAME_WIDTH 320
#define GAME_HEIGHT 200
#define GAME_RACING_HEIGHT 120
#define GAME_RACING_HORIZON 60  // Fixed horizon position (where sky meets road)
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

#include "types.h"
#include "driver.h"
#include "SFont.h"
#include "utils.h"

extern SDL_Window *win;
extern SDL_Renderer *renderer;
extern bool isFullScreen;
