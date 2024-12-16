#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"
#include "utils.h"
#include "bitmap.h"
#include "spritesheet.h"

class GasStation : public CarScene {
   private:
    sdl2::SurfacePtr maskedBackground;
    sdl2::TexturePtr maskedBackgroundTexture;
    Spritesheet* attendant;
    Bitmap* pumpingHandle;

    bool isPumping;

   public:
    GasStation();
    void Init(bool initialDebugMode) override;

    SDL_Point GetCarIdlePosition() override;
    SDL_Point GetCarOffscreenPosition() override;

    void Action(ZoneId zone) override;
    void Draw() override;
    void UpdateDebugInfo() override;

    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnFadeComplete() override;

    Bitmap* maskBitmap;
};
