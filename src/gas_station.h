#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"
#include "bitmap.h"
#include "utils.h"

class GasStation : public CarScene {
   private:
    sdl2::SurfacePtr maskedBackground;
    sdl2::TexturePtr maskedBackgroundTexture;

    bool isPumping;

   public:
    GasStation();
    void Init(bool initialDebugMode) override;

    MousePointer GetPointerType() override;

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;

    void Action(ZoneId zone) override;
    void Draw() override;

    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnFadeComplete() override;

    Bitmap* maskBitmap;
};
