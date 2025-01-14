#pragma once

#include "global.h"
#include "scene.h"
#include "mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "ui/list_dialog.h"
#include "carbitmap.h"

class Garage : public CarScene {
   private:
    Car *pendingCar = nullptr;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    std::unique_ptr<ListDialog> carSelectDialog;
    std::unique_ptr<Dialog> gameMenuDialog;
    // Container for all car bitmaps
    std::map<Car*, std::unique_ptr<CarBitmap>> carBitmaps;

    void InitGameMenuDialog();
    void UpdateSign();

   public:
    Garage();
    ~Garage();
    
    void InitCarBitmaps();  // Create bitmaps for all owned cars
    CarBitmap* GetOrCreateCarBitmap(Car* car);

    SDL_Point GetCarIdlePosition() override;

    void Action(ZoneId zone) override;
    void PushEvent(SDL_Event* event) override;
    void Draw() override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
