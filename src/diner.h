#pragma once

#include "global.h"
#include "state.h"
#include "scene.h"
#include "spritesheet.h"

class Diner: public Scene {
   private:
    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    bool guyIdle;
    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    Uint64 t;

    private:
        void CycleGuy();

    public:
        Diner();
        SDL_Point GetCarIdlePosition() override;
        void Draw() override;
};