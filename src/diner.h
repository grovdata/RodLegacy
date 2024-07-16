#pragma once

#include "scene.h"
#include "spritesheet.h"
#include "carbitmap.h"

class Diner: public CarScene {
   private:
    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 155, 320, 10}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 10};

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    Uint64 t;

    void CycleGuy();

    public:
        Diner();
        ~Diner();
        void Init() override;

        MousePointer GetPointerType() override;

        SDL_Point GetCarIdlePosition() override;
        SDL_Point GetCarOffscreenPosition() override;

        void Action(ZoneId zone) override;

        void Draw() override;
        void DrawMaskAfterCar() override;
        // void SetStatusBar(char *text) override;

        void StartAnimateCar(Car *car, Driver *driver, bool keepDriving) override;

        // Used by animation callback
        bool animatingCar = false;
        int carx, cary;

        static Uint32 CarAnimationTimerCallback(Uint32 interval, void* param);
};
