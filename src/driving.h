#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"

class Driving: public Scene {
   private:

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect = {0, 155, 320, 10}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 10};

    CarBitmap *carBitmap;

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    Uint64 t;

    SDL_TimerID opponentCarTimerID;
    static Uint32 CarAnimationTimerCallback(Uint32 interval, void* param);

    void CycleGuy();

    public:
        Driving();
        ~Driving();

        MousePointer GetPointerType() override;

        SDL_Point GetCarIdlePosition() override;
        SDL_Point GetCarOffscreenPosition() override;

        void Action(ZoneId zone) override;

        void Draw() override;
        void DrawMaskAfterCar() override;
        // void SetStatusBar(char *text) override;

        void StartAnimateCar(Car *car, Driver *driver, bool keepDriving) override;

        // CRAP
        bool animatingCar = false;
        int carx, cary;
        SDL_Point CarPos();
};