#include "diner.h"
#include "state.h"
#include "cardata.h"

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;
const double WAIT_BEFORE_EXIT = 3.0;  // seconds to wait before exit
const double WAIT_BEFORE_NEXT = 2.0;  // seconds to wait before next car

Diner::Diner() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, DINER_SCREEN}) {

}

void Diner::Init() {
    Scene::Init();

    roadMaskSurface = SDL_CreateRGBSurface(0, 320, 10, 32, 0, 0, 0, 0);
    SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface, &roadMaskDestRect);
    roadMaskTexture = SDL_CreateTextureFromSurface(renderer, roadMaskSurface);
    SDL_SetTextureBlendMode(roadMaskTexture, SDL_BLENDMODE_NONE);
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

    phonebookGuy = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, PHONEBOOK_GUY});
    phonebookGuy->SetPosition(189, 65);
    phonebookGuy->SetDimensions(38, 60);
    phonebookGuy->SetVisible(true);
    phonebookGuy->SelectSprite(0, 0);

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
    if (!isSetup) {
        Init();
        isSetup = true;
    }

    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        phonebookGuy->CycleNextSprite();
    }

    phonebookGuy->Draw();

    // Handle car animation states
    if (!animatingCar && !exitingCar && carBitmap != nullptr) {
        waitTimer += 1.0/60.0;  // Assuming 60 FPS
        if (waitTimer >= WAIT_BEFORE_EXIT) {
            exitingCar = true;
            waitTimer = 0;
            carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);
        }
    }
    
    if (!animatingCar && !exitingCar && carBitmap == nullptr) {
        waitTimer += 1.0/60.0;
        if (waitTimer >= WAIT_BEFORE_NEXT) {
            waitTimer = 0;
            StartAnimateCar(state.GetCurrentCar(), nullptr, false);
            animatingCar = true;
        }
    }

    if (elapsed > 5000 && !animatingCar && !exitingCar && carBitmap == nullptr) {
        StartAnimateCar(state.GetCurrentCar(), nullptr, false);
        animatingCar = true;
    }

    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; 
        p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();
    }

    for (auto b : bubbles) {
        b->Draw();
    }
}

void Diner::DrawMaskAfterCar() {
    SDL_BlitSurface(surface, &roadMaskSrcRect, roadMaskSurface, &roadMaskDestRect);
    roadMaskTexture = SDL_CreateTextureFromSurface(renderer, roadMaskSurface);
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

string Diner::GetOpponentText(Opponent *opponent) {
    string result = opponent->driver.name + "    " + opponent->car->GetModel().name;
    return result;
}

void Diner::StartAnimateCar(Car* car, Driver* driver, bool keepDriving) {
    if (animatingCar && carBitmap) return;
    // From announcing the driver with text, it should take approx. 8 seconds to
    // drive the car to a halt.
    Opponent *opponent = state.GetNextOpponent();
    string oppoText = GetOpponentText(opponent);
    Car *carToAnimate = opponent->car;
    
    ClearStatusBar();
    SetStatusBar(strdup(oppoText.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(oppoText.c_str()));

    if (carToAnimate != nullptr && carBitmap == nullptr) {
        carBitmap = new CarBitmap(carToAnimate);
        carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);  // 16 ms interval for about 60 FPS
    }
}

Uint32 Diner::CarAnimationTimerCallback(Uint32 interval, void* param) {
    Diner* diner = static_cast<Diner*>(param);
    
    if (diner->exitingCar) {
        carExitAnimationTime += interval / 1000.0;
        const double exitDuration = 2.0;
        double easedValue = (1 - pow(2, -10 * carExitAnimationTime / exitDuration));
        
        // Move from current position to off right side of screen
        int startX = 60;  // Starting X position
        int endX = 320;   // End X position (off screen)
        int carX = static_cast<int>(startX + (easedValue * (endX - startX)));
        diner->carx = carX;

        if (easedValue >= 0.992) {
            diner->exitingCar = false;
            diner->carBitmap = nullptr;  // Clear the current car
            carExitAnimationTime = 0;
            return 0;
        }
        return interval;
    }
    
    // Original entrance animation
    carAnimationTimeDiner += interval / 1000.0;
    const double duration = 2.0;
    double easedValue = (1 - pow(2, -10 * carAnimationTimeDiner / duration));
    int carX = static_cast<int>((easedValue * (320 + 50)) - 320);
    int carY = 100;
    diner->carx = carX;
    diner->cary = carY;

    if (easedValue >= 0.992) {
        diner->animatingCar = false;
        carAnimationTimeDiner = 0;
        return 0;
    }
    return interval;
}
