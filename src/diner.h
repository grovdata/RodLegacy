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

    private:
        void CycleGuy();

    public:
        Diner();
        ~Diner();
        SDL_Point GetCarIdlePosition() override;
        void Draw() override;
        void DrawMaskAfterCar() override;
};