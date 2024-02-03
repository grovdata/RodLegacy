#pragma once

#include "global.h"
#include "scene.h"

class Garage : public Scene {
   public:
    Garage();

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;
};
