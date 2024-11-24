#include "diner.h"
#include "state.h"
#include "cardata.h"
#include "debug_overlay.h"

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;
const double WAIT_BEFORE_EXIT = 3.0;  // seconds to wait before exit
const double WAIT_BEFORE_NEXT = 2.0;  // seconds to wait before next car

Diner::Diner() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, DINER_SCREEN}) {
    activeCarBitmap = nullptr;
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
    opponentCarBitmaps.clear();
    activeCarBitmap = nullptr;
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
    if (!animatingCar && !exitingCar && activeCarBitmap) {
        waitTimer += 1.0/60.0;  // Assuming 60 FPS
        if (waitTimer >= WAIT_BEFORE_EXIT) {
            exitingCar = true;
            waitTimer = 0;
            carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);
        }
    }
    
    if (!animatingCar && !exitingCar && !activeCarBitmap) {
        waitTimer += 1.0/60.0;
        if (waitTimer >= WAIT_BEFORE_NEXT) {
            waitTimer = 0;
            StartAnimateCar(state.GetCurrentCar(), nullptr, false);
            animatingCar = true;
        }
    }

    if (elapsed > 5000 && !animatingCar && !exitingCar && !activeCarBitmap) {
        StartAnimateCar(state.GetCurrentCar(), nullptr, false);
        animatingCar = true;
    }

    if (activeCarBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; 
        p.y = cary;
        activeCarBitmap->Move(p);
        activeCarBitmap->Draw();
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
    if (animatingCar && activeCarBitmap) return;
    
    Opponent *opponent = state.GetNextOpponent();
    if (!opponent) return;
    
    string oppoText = GetOpponentText(opponent);
    string opponentId = opponent->driver.name;  // Or some unique identifier for the opponent
    
    ClearStatusBar();
    SetStatusBar(strdup(oppoText.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(oppoText.c_str()));

    // Check if we already have a CarBitmap for this opponent
    auto existingBitmap = opponentCarBitmaps.find(opponentId);
    if (existingBitmap != opponentCarBitmaps.end()) {
        activeCarBitmap = existingBitmap->second.get();
    } else {
        // Create new CarBitmap and store it
        auto newBitmap = std::make_unique<CarBitmap>(opponent->car, &opponent->driver);
        activeCarBitmap = newBitmap.get();
        opponentCarBitmaps[opponentId] = std::move(newBitmap);
    }

    carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);
    animatingCar = true;
}

Uint32 Diner::CarAnimationTimerCallback(Uint32 interval, void* param) {
    Diner* diner = static_cast<Diner*>(param);
    
    if (diner->exitingCar) {
        carExitAnimationTime += interval / 1000.0;
        const double exitDuration = 1.5;
        double easedValue = (1 - pow(2, -10 * carExitAnimationTime / exitDuration));
        
        // Move from current position to off right side of screen
        int startX = 60;  // Starting X position
        int endX = GAME_WIDTH;   // End X position (off screen)
        int carX = static_cast<int>(startX + (easedValue * (endX - startX)));
        diner->carx = carX;

        // Calculate instantaneous speed for wheel animation
        float speed = (endX - startX) * pow(2, -10 * carExitAnimationTime / exitDuration) * (10.0/exitDuration);
        if (diner->activeCarBitmap) {
            diner->activeCarBitmap->UpdateWheelAnimation(speed);
        }

        if (easedValue >= 0.992) {
            diner->exitingCar = false;
            diner->activeCarBitmap = nullptr;  // Don't delete, just clear the pointer
            carExitAnimationTime = 0;
            return 0;
        }
        return interval;
    }
    
    // Original entrance animation
    carAnimationTimeDiner += interval / 1000.0;
    const double duration = 2.0;
    double easedValue = (1 - pow(2, -10 * carAnimationTimeDiner / duration));
    int carX = static_cast<int>((easedValue * (GAME_WIDTH + 50)) - GAME_WIDTH);
    int carY = 100;
    diner->carx = carX;
    diner->cary = carY;

    // Calculate instantaneous speed for wheel animation
    float speed = (GAME_WIDTH + 50) * pow(2, -10 * carAnimationTimeDiner / duration) * (10.0/duration);
    if (diner->activeCarBitmap) {
        diner->activeCarBitmap->UpdateWheelAnimation(speed);
    }

    if (easedValue >= 0.992) {
        diner->animatingCar = false;
        carAnimationTimeDiner = 0;
        return 0;
    }
    return interval;
}

void Diner::UpdateDebugInfo() {
    if (!debugMode) return;
    
    CarScene::UpdateDebugInfo();
    
    // Use inherited debugOverlay
    // debugOverlay->AddLine(std::string("Animation: ") + (animatingCar ? "true" : "false"));
    // debugOverlay->AddLine(std::string("Exiting: ") + (exitingCar ? "true" : "false"));

    if (debugOverlay) {
    debugOverlay->Clear();
    //debugOverlay->AddLine("Scene: " + std::string(typeid(*this).name()));

    debugOverlay->AddLine("Time: " + std::to_string(SDL_GetTicks64()));    
    debugOverlay->AddLine("Car X: " + std::to_string(carx));
    debugOverlay->AddLine("Car Y: " + std::to_string(cary));
    Opponent *opponent = state.GetNextOpponent();   
    if (opponent) {        
        debugOverlay->AddLine("Opponent: " + opponent->driver.name);
    }
    }
}
