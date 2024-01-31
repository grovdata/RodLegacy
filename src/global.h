#pragma once
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

using namespace std;

#include "SFont.h"
#include "game.h"
#include "parsers/libp.h"
#include "ui/button.h"
#include "ui/zone.h"
#include "spritesheet.h"

extern SDL_Renderer *renderer;
extern SFont_Font *font;
