#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"
#include "bitmap.h"


class GasStation : public CarScene {
   public:
    GasStation();

    MousePointer GetPointerType() override;

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;

    void Action(ZoneId zone) override;
    void Draw() override;

    void StartAnimateCar(Car* car, Driver* driver, bool keepDriving) override;

    static Uint32 CarAnimationTimerCallback(Uint32 interval, void* param);

    Bitmap* maskBitmap;
};
