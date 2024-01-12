#pragma once

#include "global.h"
#include "scene.h"

class Diner : public Scene {
   public:
    Diner(Game *game) : Scene(game, "../data/rip/diner.png") {
        SDL_Rect orderRect = {132, 53, 27, 9};
        Zone order(strdup("Order"), &orderRect);

        SDL_Rect opponentRect = {194, 67, 29, 59};
        Zone opponent(strdup("Look for opponent"), &opponentRect);

        SDL_Rect garageRect = {0, 154, 320, 34};
        Zone garage(strdup("Back to the garage"), &garageRect);

        m_zones.push_back(order);
        m_zones.push_back(opponent);
        m_zones.push_back(garage);
    }

    SDL_Point GetCarIdlePosition() {
        SDL_Point p = {60, 95};
        return p;
    }
};