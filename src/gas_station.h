#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"
#include "utils.h"
#include "bitmap.h"
#include "spritesheet.h"

enum GasStationState {
    GAS_STATION_IDLE,
    GAS_STATION_HANDLEOFF,
    GAS_STATION_PUMPING
};

class GasStation : public CarScene {
   private:
    sdl2::SurfacePtr maskedBackground;
    sdl2::TexturePtr maskedBackgroundTexture;
    sdl2::SurfacePtr roadMaskSurface;
    sdl2::TexturePtr roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 175, 320, 15}; // Mask from y=175 down, 15 pixels high
    SDL_Rect roadMaskDestRect = {0, 0, 320, 15};
    GasStationState sceneState = GAS_STATION_IDLE;
    std::unique_ptr<CarBitmap> currentCarBitmap;
    void InitCurrentCarBitmap();
    CarBitmap* GetOrCreateCurrentCarBitmap();

   public:
    GasStation();
    ~GasStation();
    void Init(bool initialDebugMode) override;

    SDL_Point GetCarIdlePosition() override;

    void Action(ZoneId zone) override;
    void Draw() override;
    void DrawMaskAfterCar() override;
    void UpdateCarAnimation(double deltaTime) override;
    void OnCarAnimationComplete() override;
    void UpdateDebugInfo() override;
    void PushEvent(SDL_Event* event) override;

    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnFadeComplete() override;
    void FadeOut() override;
    void OnSceneExit() override;

};
