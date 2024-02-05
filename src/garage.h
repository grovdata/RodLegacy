#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"

class Garage : public Scene {
   private:
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    void UpdateSign();

   public:
    Garage();

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;

    void Action(ZoneId zone) override;
    void Draw() override;
};
