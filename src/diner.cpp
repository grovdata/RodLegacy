#include "diner.h"
#include "state.h"
#include "cardata.h"

double carAnimationTimeDiner = 0;

Diner::Diner() : CarScene("../data/res/diner3.gif") {
    roadMaskSurface = SDL_CreateRGBSurface(0, 320, 10, 32, 0, 0, 0, 0);
    SDL_BlitSurface(surface, &roadMaskSrcRect, roadMaskSurface, &roadMaskDestRect);
    roadMaskTexture = SDL_CreateTextureFromSurface(renderer, roadMaskSurface);
    SDL_SetTextureBlendMode(roadMaskTexture, SDL_BLENDMODE_BLEND);
    textures.push_back(roadMaskTexture);

    SDL_Rect orderRect = {132, 53, 27, 9};
    Zone order(ZONE_DINER_ORDER, strdup("Order"), &orderRect);

    SDL_Rect opponentRect = {194, 65, 29, 59};
    Zone opponent(ZONE_DINER_CALL, strdup("Look for opponent"), &opponentRect);

    SDL_Rect garageRect = {0, 170, 320, 30};
    Zone garage(ZONE_DINER_GOTO_GARAGE, strdup("Back to the garage"), &garageRect);

    SDL_Rect backgroundRect = {0, 0, 320, 169};
    Zone backgroundZone(ZONE_DINER_BACKGROUND, strdup(""), &backgroundRect);

    zones.push_back(order);
    zones.push_back(opponent);
    zones.push_back(garage);
    zones.push_back(backgroundZone);

    Bubble *guyBubble = new Bubble("  What\'s his name?", BUBBLE_LEFT, 79, 30, true);
    guyBubble->SetVisible(false);
    bubbles.push_back(guyBubble);

    phonebookGuy = new Spritesheet("../data/res/lib_p_315.gif", 2, 5);
    waitress = new Spritesheet("../data/res/lib_p_335.gif", 2, 6);
    textures.push_back(phonebookGuy->GetTexture());
    textures.push_back(waitress->GetTexture());

    guySpriteX = 0;
    guySpriteY = 0;

    t = SDL_GetTicks64();
}

Diner::~Diner() {
    SDL_FreeSurface(roadMaskSurface);
    SDL_DestroyTexture(roadMaskTexture);
    SDL_RemoveTimer(carTimerID);
}

MousePointer Diner::GetPointerType() {
    return PointerHand;
}

void Diner::CycleGuy() {
    if (guySpriteX == 4) {
        guySpriteX = 0;
        guySpriteY = (guySpriteY == 1) ? 0 : 1;
    } else {
        guySpriteX += 1;
    }
    // printf("Guy %d %d\n", guySpriteX, guySpriteY);
}

SDL_Point Diner::GetCarIdlePosition() {
    return {60, 95};
}

SDL_Point Diner::GetCarOffscreenPosition() {
    return {-320, 95};
}

void Diner::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    if (zone == ZONE_DINER_GOTO_GARAGE) {
        sceneTransitioningTo = SCENE_GARAGE;
        FadeOut();
    } else if (zone == ZONE_DINER_CALL) {
        bubbles[0]->SetVisible(true);
    }
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

    if (elapsed > 2000 && !animatingCar) {
        // printf("start animate\n");
        StartAnimateCar(state.GetCurrentCar(), nullptr, false);
        animatingCar = true;
    }
    if (carBitmap != nullptr) {
        // printf("Move %d %d\n",carx,cary);
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();
    }
    for (auto b : bubbles) {
        b->Draw();
    }
}

void Diner::DrawMaskAfterCar() {
    SDL_RenderCopy(renderer, roadMaskTexture, NULL, &roadMaskSrcRect);
    CarScene::DrawMaskAfterCar();
}

// void Diner::SetStatusBar(char *text) {
//     char statusText[70];
//     snprintf(statusText, 30, "Bankroll: $%d", state.GetBankroll());
//     SFont_Write(fontSurface, font, 2, 200 - 9, statusText);
//     SFont_Write(fontSurface, font, 160, 200 - 9, text);
//     fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
//     SDL_SetTextureColorMod(fontTexture, 119, 102, 17);
// }

void Diner::StartAnimateCar(Car* car, Driver* driver, bool keepDriving) {
    if (animatingCar && carBitmap) return;
    // From announcing the driver with text, it should take approx. 8 seconds to
    // drive the car to a halt.
    Car *carToAnimate = state.GetCurrentCar();
    CarModel model = carToAnimate->GetModel();
    SetStatusBar(strdup(model.name.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(model.name.c_str()));

    if (carToAnimate != nullptr && carBitmap == nullptr) {
        printf("create new car \n");
        carBitmap = new CarBitmap(carToAnimate);
        carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);  // 16 ms interval for about 60 FPS
    }
}

Uint32 Diner::CarAnimationTimerCallback(Uint32 interval, void* param) {
    Diner* diner = static_cast<Diner*>(param);
    carAnimationTimeDiner += interval / 1000.0;
    const double duration = 2.0;
    // Ease-out function (you can experiment with different easing functions)
    double easedValue = (1 - pow(2, -10 * carAnimationTimeDiner / duration));
    // printf("easedvalue %f\n",easedValue);
    int carX = static_cast<int>((easedValue * (320 + 50)) - 320);  // Scale to cover the range -320 to 100
    // int carX = (int)(easedValue * 100);
    int carY = 100;
    diner->carx = carX;
    diner->cary = carY;

    if (easedValue >= 0.992) {
        // printf("stop\n");
        diner->animatingCar = false;
        return 0;
    }
    // printf("Carx cary %d %d\n",carX,carY);
    return interval;
}