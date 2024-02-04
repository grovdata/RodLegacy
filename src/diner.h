#pragma once

#include "scene.h"
#include "spritesheet.h"
#include "carbitmap.h"

class Diner: public Scene {
   private:
    //Car *currentCar;

    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect = {0, 200-45, 320, 20}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 200-45-11};

    CarBitmap *carBitmap;

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

        void Action(ZoneId zone) override;

        void Draw() override;
        void DrawMaskAfterCar() override;

        void StartAnimateCar(Car *car, Driver *driver, bool keepDriving) override;

        // CRAP
        bool animatingCar = false;
        int carx, cary;
        SDL_Point CarPos();
};