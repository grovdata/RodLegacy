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

    void CycleGuy() {
        if (guySpriteX == 5 && guySpriteY == 1) {
            guySpriteX = 1;
            guySpriteY = 0;
            return;
        }
        if (guySpriteX == 5 && guySpriteY == 0) {
            guySpriteX = 1;
            guySpriteY = 1;
        }
        guySpriteX += 1;
    }


   public:
    Diner(Game *game) : Scene(game, "../data/rip/diner2.png") {
        SDL_Rect orderRect = {132, 53, 27, 9};
        Zone order(strdup("Order"), &orderRect);

        SDL_Rect opponentRect = {194, 65, 29, 59};
        Zone opponent(strdup("Look for opponent"), &opponentRect);

        SDL_Rect garageRect = {0, 154, 320, 34};
        Zone garage(strdup("Back to the garage"), &garageRect);

        m_zones.push_back(order);
        m_zones.push_back(opponent);
        m_zones.push_back(garage);

        phonebookGuy = new Spritesheet("../data/rip/lib_p_315.gif", 5, 2);

        guySpriteX = 1;
        guySpriteY = 0;

        t = SDL_GetTicks64();
    }

    SDL_Point GetCarIdlePosition() {
        SDL_Point p = {60, 95};
        return p;
    }

    void Draw() override {
        Scene::Draw();

        Uint64 elapsed = SDL_GetTicks64() - t;

        if (elapsed % 100 == 0) {
            CycleGuy();
        }

        phonebookGuy->SelectSprite(guySpriteX, guySpriteY);
        phonebookGuy->DrawSprite(&guyRect);
    }
};