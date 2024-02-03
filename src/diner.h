#pragma once

#include "global.h"
#include "state.h"
#include "scene.h"


class Diner: public Scene {
   private:
    Car *currentCar;

    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect = {0, 200-45, 320, 20}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 200-45-11};

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    Uint64 t;


    SDL_TimerID opponentCarTimerID;
    static Uint32 CarAnimationTimerCallback(Uint32 interval, void* param);

    void CycleGuy();

    public:
        Diner();
        ~Diner();
        SDL_Point GetCarIdlePosition() override;
        SDL_Point GetCarOffscreenPosition() override;

        void Draw() override;
        void DrawMaskAfterCar() override;

        void StartAnimateCar(CarModel *car, Driver *driver, bool keepDriving) override;
    // CRAP
    int carx, cary;
    SDL_Point CarPos();
};