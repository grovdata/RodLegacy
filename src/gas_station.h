#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"

class GasStation : public Scene {
   public:
    GasStation();

    MousePointer GetPointerType() override;

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;

    void Action(ZoneId zone) override;
};
