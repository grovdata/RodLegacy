#pragma once

#include "global.h"
#include "scene.h"
#include "mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "ui/car_select_dialog.h"

class Garage : public CarScene {
   private:
    bool pendingCarSwap = false;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    void UpdateSign();
    std::unique_ptr<CarSelectDialog> carSelectDialog;

   public:
    Garage();

    SDL_Point GetCarIdlePosition() override;

    void Action(ZoneId zone) override;
    void PushEvent(SDL_Event* event) override;
    void Draw() override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
