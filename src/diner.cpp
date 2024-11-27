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
    isPaused = false;
}

void Diner::Init(bool debugMode) {
    CarScene::Init(debugMode);

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
    return {80, 95};
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
    if (debugMode && isPaused) {
        // When paused, only redraw the scene without updating car animations or opponent logic
        Scene::Draw();  // Draw background
        
        // Draw the car in its current position
        if (activeCarBitmap) {
            activeCarBitmap->Draw();
        }
        
        // Draw any existing opponent car
        Opponent* opponent = state.GetCurrentOpponent();
        if (opponent && opponentCarBitmaps.count(opponent->driver.name)) {
            opponentCarBitmaps[opponent->driver.name]->Draw();
        }
        
        // Draw debug overlay if active
        if (debugOverlay && debugMode) {
            debugOverlay->Draw();
        }
        
        return;
    }

    // Normal non-paused drawing and update logic
    Scene::Draw();
    if (!isSetup) {
        Init(debugMode);
        isSetup = true;
    }

    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        phonebookGuy->CycleNextSprite();
    }

    phonebookGuy->Draw();

    // Handle car animation states
    if (!animatingCar && !exitingCar && activeCarBitmap) {
        waitTimer += 1.0/60.0;
        if (waitTimer >= WAIT_BEFORE_EXIT) {
            if (debugMode) {
                printf("Starting Exit Animation\n");
            }
            exitingCar = true;
            waitTimer = 0;
            carExitAnimationTime = 0;
            carTimerID = SDL_AddTimer(8, CarAnimationTimerCallback, this);
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
        activeCarBitmap->Move({carx, cary});
        activeCarBitmap->Draw();
    }

    for (auto b : bubbles) {
        b->Draw();
    }

    if (debugMode && debugOverlay) {
        debugOverlay->Draw();
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
    
    this->currentOpponent = state.GetNextOpponent();
    if (!currentOpponent) return;
    
    string oppoText = GetOpponentText(currentOpponent);
    string opponentId = currentOpponent->driver.name;  // Or some unique identifier for the opponent
    
    ClearStatusBar();
    SetStatusBar(strdup(oppoText.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(oppoText.c_str()));

    // Check if we already have a CarBitmap for this opponent
    auto existingBitmap = opponentCarBitmaps.find(opponentId);
    if (existingBitmap != opponentCarBitmaps.end()) {
        activeCarBitmap = existingBitmap->second.get();
    } else {
        // Create new CarBitmap and store it
        StandardCarColor carColor = static_cast<StandardCarColor>(currentOpponent->car->GetColor());
        auto newBitmap = std::make_unique<CarBitmap>(currentOpponent->car, &currentOpponent->driver, carColor);
        activeCarBitmap = newBitmap.get();
        opponentCarBitmaps[opponentId] = std::move(newBitmap);
    }

    int carYOffset = state.carData.GetCarRoofYOffset(currentOpponent->car->GetModel());    
    this->cary = carYOffset;

    int carWheelsYOffset = state.carData.GetCarWheelsYOffset(currentOpponent->car->GetModel());
    activeCarBitmap->MoveWheelsYOffset(carWheelsYOffset);

    carTimerID = SDL_AddTimer(8, CarAnimationTimerCallback, this);
    animatingCar = true;
}

Uint32 Diner::CarAnimationTimerCallback(Uint32 interval, void* param) {
    Diner* diner = static_cast<Diner*>(param);
    static Uint64 lastTime = SDL_GetTicks64();
    Uint64 currentTime = SDL_GetTicks64();
    double deltaTime = (currentTime - lastTime) / 1000.0;
    lastTime = currentTime;
    
    if (diner->exitingCar) {
        carExitAnimationTime += deltaTime;
        const double exitDuration = 2.5;  // Increased duration for smoother exit
        
        // Cubic ease-in: t³ (starts slow, accelerates)
        double t = carExitAnimationTime / exitDuration;
        t = std::min(1.0, t);
        double easedValue = t * t * t;  // Cubic easing
        
        SDL_Point idlePos = diner->GetCarIdlePosition();
        int startX = idlePos.x;
        int endX = GAME_WIDTH + 50;
        int newX = static_cast<int>(startX + (easedValue * (endX - startX)));
        
        if (diner->debugMode) {
            printf("Exit Animation: time=%.2f, eased=%.2f, x=%d->%d (current=%d)\n", 
                   carExitAnimationTime, easedValue, startX, endX, newX);
        }
        
        diner->carx = newX;

        // Wheel speed matches the acceleration
        float speed = (endX - startX) * (3 * t * t) / exitDuration;  // Derivative of cubic for velocity
        if (diner->activeCarBitmap) {
            diner->activeCarBitmap->UpdateWheelAnimation(speed * deltaTime);
            diner->activeCarBitmap->Move({diner->carx, diner->cary});
        }

        if (carExitAnimationTime >= exitDuration) {
            if (diner->debugMode) {
                printf("Exit Animation Complete\n");
            }
            diner->exitingCar = false;
            diner->activeCarBitmap = nullptr;
            carExitAnimationTime = 0;
            SDL_RemoveTimer(diner->carTimerID);
            diner->carTimerID = 0;
            return 0;
        }
        return interval;
    }
    
    // Entrance animation
    carAnimationTimeDiner += deltaTime;
    const double duration = 2.0;
    double easedValue = (1 - pow(2, -10 * carAnimationTimeDiner / duration));
    
    // Smoother position calculation
    diner->carx = static_cast<int>((easedValue * (GAME_WIDTH + 50)) - GAME_WIDTH);

    // Smoother wheel animation
    float speed = (GAME_WIDTH + 50) * pow(2, -10 * carAnimationTimeDiner / duration) * (10.0/duration);
    if (diner->activeCarBitmap) {
        diner->activeCarBitmap->UpdateWheelAnimation(speed * deltaTime);
    }

    if (carAnimationTimeDiner >= duration) {
        diner->animatingCar = false;
        carAnimationTimeDiner = 0;
        return 0;
    }
    return interval;
}

void Diner::UpdateDebugInfo() {
    if (!debugMode) return;
    
    CarScene::UpdateDebugInfo();
        
    if (debugOverlay) {
        debugOverlay->Clear();
        
        debugOverlay->AddLine("Car X: " + std::to_string(carx) + " Y: " + std::to_string(cary));        

        if (currentOpponent && activeCarBitmap) {        
            debugOverlay->AddLine("Opponent: " + currentOpponent->driver.name);
            CarModel model = currentOpponent->car->GetModel();
            BitmapOffset_t offset = model.offsets;
            debugOverlay->AddLine("model: " + model.name + " color: " + std::to_string(currentOpponent->car->GetColor()));
            debugOverlay->AddLine("roof chopped: " + std::string(currentOpponent->car->IsRoofChopped() ? "Yes" : "No"));
            debugOverlay->AddLine("bumpers stripped: " + std::string(currentOpponent->car->IsBumpersStripped() ? "Yes" : "No"));
            debugOverlay->AddLine("offsetXAirScoop: " + std::to_string(offset.offsetXAirScoop) + " Y: " + std::to_string(offset.offsetYAirScoop));            
            debugOverlay->AddLine("offsetXRearTire: " + std::to_string(offset.offsetXRearTire));
            debugOverlay->AddLine("offsetXFrontTire: " + std::to_string(offset.offsetXFrontTire));
            debugOverlay->AddLine("offsetYTires: " + std::to_string(activeCarBitmap->GetWheelsYOffset()));
            debugOverlay->AddLine("offsetXDriver: " + std::to_string(offset.offsetXDriver) + " Y: " + std::to_string(offset.offsetYDriver));            
            debugOverlay->AddLine("offsetXChoppedRoof: " + std::to_string(offset.offsetXChoppedRoof));
            debugOverlay->AddLine("offsetXFrontBumper: " + std::to_string(offset.offsetXFrontBumper) + " Y: " + std::to_string(offset.offsetYFrontBumper));            
            debugOverlay->AddLine("offsetXDecal: " + std::to_string(offset.offsetXDecal) + " Y: " + std::to_string(offset.offsetYDecal));
            
        }
        
        debugOverlay->AddLine("Paused: " + std::string(isPaused ? "Yes" : "No"));
    }
}

void Diner::PushEvent(SDL_Event *event) {
    CarScene::PushEvent(event);  // Handle parent events first (like F12)

    if (!debugMode) return;  // Only handle these events in debug mode

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_p:
                isPaused = !isPaused;
                break;
            
            case SDLK_UP:
                if (isPaused && activeCarBitmap) {
                    cary--;  // Move car up
                    activeCarBitmap->MoveBody({carx, cary});
                }
                break;
                
            case SDLK_DOWN:
                if (isPaused && activeCarBitmap) {
                    cary++;  // Move car down
                    activeCarBitmap->MoveBody({carx, cary});
                }
                break;

            case SDLK_LEFT:
                if (isPaused && activeCarBitmap) {
                    activeCarBitmap->MoveWheelsYOffset(activeCarBitmap->GetWheelsYOffset() - 1);
                    
                }
                break;

            case SDLK_RIGHT:
                if (isPaused && activeCarBitmap) {
                    activeCarBitmap->MoveWheelsYOffset(activeCarBitmap->GetWheelsYOffset() + 1);
                    
                }
                break;

            case SDLK_r:
                if (isPaused && activeCarBitmap) {
                    activeCarBitmap->ToggleChoppedRoof();
                }
                break;
        }
    }
}

void Diner::OnFadeComplete() {
    // Start initial car animation
    if (!animatingCar && !exitingCar && !activeCarBitmap) {
        StartAnimateCar(state.GetCurrentCar(), nullptr, false);
        animatingCar = true;
    }
}