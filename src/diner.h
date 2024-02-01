#pragma once

#include "global.h"
#include "state.h"
#include "scene.h"


class Diner: public Scene {
   private:
    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect = {0, 155, 320, 200-155};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 200-155};

    bool guyIdle;
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

        virtual void StartAnimateCar(Car *car, Driver *driver, bool keepDriving) override;
    // CRAP
    int carx, cary;
    SDL_Point CarPos();
};