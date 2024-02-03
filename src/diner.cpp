#include "diner.h"

double carAnimationTime = 0;

void Diner::CycleGuy() {
    if (guySpriteX == 4) {
        guySpriteX = 0;
        guySpriteY = (guySpriteY == 1) ? 0 : 1;
    } else {
        guySpriteX += 1;
    }
    // printf("Guy %d %d\n", guySpriteX, guySpriteY);
}

Diner::Diner() : Scene("../data/res/diner3.gif") {
    roadMaskSurface = SDL_CreateRGBSurface(0, 320, 55, 32, 0, 0, 0, 0);
    SDL_BlitSurface(m_Surface, &roadMaskSrcRect, roadMaskSurface, &roadMaskDestRect);
    roadMaskTexture = SDL_CreateTextureFromSurface(renderer, roadMaskSurface);

    SDL_Rect orderRect = {132, 53, 27, 9};
    Zone order(ZONE_DINER_ORDER, strdup("Order"), &orderRect);

    SDL_Rect opponentRect = {194, 65, 29, 59};
    Zone opponent(ZONE_DINER_CALL, strdup("Look for opponent"), &opponentRect);

    SDL_Rect garageRect = {0, 154, 320, 34};
    Zone garage(ZONE_DINER_GOTO_GARAGE, strdup("Back to the garage"), &garageRect);

    m_zones.push_back(order);
    m_zones.push_back(opponent);
    m_zones.push_back(garage);

    phonebookGuy = new Spritesheet("../data/res/lib_p_315.gif", 2, 5);
    waitress = new Spritesheet("../data/res/lib_p_335.gif", 2, 6);

    guySpriteX = 0;
    guySpriteY = 0;

    t = SDL_GetTicks64();
}

Diner::~Diner() {
    SDL_FreeSurface(roadMaskSurface);
    SDL_DestroyTexture(roadMaskTexture);
    SDL_RemoveTimer(opponentCarTimerID);
}

SDL_Point Diner::GetCarIdlePosition() {
    return {60, 95};
}

SDL_Point Diner::GetCarOffscreenPosition() {
    return {-320, 95};
}

void Diner::Action(ZoneId zone) {

}

void Diner::Draw() {
    Scene::Draw();

    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        CycleGuy();
        // Fix spritesheet code
        // phonebookGuy->CycleNextSprite();
    }

    phonebookGuy->SelectSprite(guySpriteX, guySpriteY);
    phonebookGuy->DrawSprite(&guyRect);

    // SetStatusBar("hej");
}

void Diner::DrawMaskAfterCar() {
    SDL_RenderCopy(renderer, roadMaskTexture, NULL, &roadMaskSrcRect);
}

void Diner::StartAnimateCar(CarModel* car, Driver* driver, bool keepDriving) {
    // From announcing the driver with text, it should take approx. 8 seconds to
    // drive the car to a halt.
    opponentCarTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);  // 16 ms interval for about 60 FPS
}

SDL_Point Diner::CarPos() {
    return {carx, cary};
}

Uint32 Diner::CarAnimationTimerCallback(Uint32 interval, void* param) {
    Diner* diner = static_cast<Diner*>(param);
    carAnimationTime += interval / 1000.0;
    const double duration = 2.0;
    // Ease-out function (you can experiment with different easing functions)
    double easedValue = (1 - pow(2, -10 * carAnimationTime / duration));
    // printf("easedvalue %f\n",easedValue);
    int carX = static_cast<int>((easedValue * (320 + 50)) - 320);  // Scale to cover the range -320 to 100
    // int carX = (int)(easedValue * 100);
    int carY = 100;
    diner->carx = carX;
    diner->cary = carY;

    if (easedValue >= 0.992) {
        // printf("stop\n");
        return 0;
    }
    // printf("Carx cary %d %d\n",carX,carY);
    return interval;
}