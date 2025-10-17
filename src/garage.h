#pragma once

#include "global.h"
#include "scene.h"
#include "mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "dialogs/list_dialog.h"
#include "carbitmap.h"
#include "engine_bay.h"

class Garage : public CarScene {
   private:
    Car *pendingCar = nullptr;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    std::unique_ptr<ListDialog> carSelectDialog;
    std::unique_ptr<Dialog> gameMenuDialog;
    std::unique_ptr<Dialog> optionsDialog;
    std::unique_ptr<Dialog> frontBumperDialog;
    std::unique_ptr<Dialog> rearBumperDialog;
    std::unique_ptr<EngineBay> engineBay;
    bool fullscreenEnabled = false;
    std::map<Car*, std::unique_ptr<CarBitmap>> carBitmaps;
    SDL_Rect signRect;
    void InitGameMenuDialog();
    void InitOptionsDialog();
    void InitBumperDialogs();
    void ShowBumperDialog(bool isFront);
    void UpdateSign();

   public:
    Garage();
    ~Garage();
    
    void InitCarBitmaps();
    CarBitmap* GetOrCreateCarBitmap(Car* car);

    SDL_Point GetCarIdlePosition() override;

    void Action(ZoneId zone) override;
    void PushEvent(SDL_Event* event) override;
    void Draw() override;
    void SetActiveDialog(Dialog *dialog) override;
    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
