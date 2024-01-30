#include "diner.h"

void Diner::CycleGuy() {
    if (guySpriteX == 4) {
        guySpriteX = 0;
        guySpriteY = (guySpriteY == 1) ? 0 : 1;
    } else {
        guySpriteX += 1;
    }
    printf("Guy %d %d\n", guySpriteX, guySpriteY);
}

Diner::Diner() : Scene("../data/rip/diner2.png") {
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

    guySpriteX = 0;
    guySpriteY = 0;

    t = SDL_GetTicks64();
}

SDL_Point Diner::GetCarIdlePosition() {
    SDL_Point p = {60, 95};
    return p;
}

void Diner::Draw() {
    Scene::Draw();

    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        CycleGuy();
    }

    phonebookGuy->SelectSprite(guySpriteX, guySpriteY);
    phonebookGuy->DrawSprite(&guyRect);
}