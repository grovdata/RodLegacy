#include "diner.h"
#include "state.h"
#include "cardata.h"
#include "debug_overlay.h"

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;

Diner::Diner() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, DINER_SCREEN}) {
    lastFrameTime = SDL_GetTicks64() / 1000.0;
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

    hud->SetVisible(true);

    initStartTime = SDL_GetTicks64();
}

Diner::~Diner() {    
    SDL_FreeSurface(roadMaskSurface);
    SDL_DestroyTexture(roadMaskTexture);
    opponentCarBitmaps.clear();
    activeCarBitmap = nullptr;
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
        state.MoveToScene(SCENE_GARAGE);
    } else if (zone == ZONE_DINER_CALL) {
        bubbles[0]->SetVisible(true);
    }
}

void Diner::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    Scene::Draw();
    if (!isSetup) {
        Init(debugMode);
        isSetup = true;
    }

    Uint64 elapsed = SDL_GetTicks64() - initStartTime;

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
            animatingCar = true;
        }
        // Still draw everything else while waiting
        phonebookGuy->Draw();
        for (auto b : bubbles) {
            b->Draw();
        }
        if (debugMode && debugOverlay) {
            debugOverlay->Draw();
        }
        return;
    }

    if (elapsed % 400 == 0) {
        phonebookGuy->CycleNextSprite();
    }

    phonebookGuy->Draw();

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
    hud->Draw();
}

void Diner::DrawMaskAfterCar() {
    SDL_BlitSurface(surface, &roadMaskSrcRect, roadMaskSurface, &roadMaskDestRect);
    roadMaskTexture = SDL_CreateTextureFromSurface(renderer, roadMaskSurface);
    SDL_RenderCopy(renderer, roadMaskTexture, NULL, &roadMaskSrcRect);
    CarScene::DrawMaskAfterCar();
}

string Diner::GetOpponentText(Opponent *opponent) {
    string result = opponent->driver.name + "    " + opponent->car->GetModel().name;
    return result;
}

void Diner::StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) {
    if (animatingCar && activeCarBitmap) return;
    
    this->currentOpponent = state.GetNextOpponent();
    if (!currentOpponent) return;
    
    string oppoText = GetOpponentText(currentOpponent);
    string opponentId = currentOpponent->driver.name;
    
    hud->SetText(strdup(oppoText.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(oppoText.c_str()));

    auto existingBitmap = opponentCarBitmaps.find(opponentId);
    if (existingBitmap != opponentCarBitmaps.end()) {
        activeCarBitmap = existingBitmap->second.get();
    } else {
        StandardCarColor carColor = static_cast<StandardCarColor>(currentOpponent->car->GetColor());
        auto newBitmap = std::make_unique<CarBitmap>(currentOpponent->car, &currentOpponent->driver, carColor);
        activeCarBitmap = newBitmap.get();
        opponentCarBitmaps[opponentId] = std::move(newBitmap);
    }

    int carYOffset = state.carData.GetCarRoofYOffset(currentOpponent->car->GetModel());
    this->cary = carYOffset;

    CarScene::StartAnimateCar(currentOpponent->car, &currentOpponent->driver, CarAnimationType::OPPONENT_CONTINUOUS);
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
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation immediately for subsequent fades
        if (!animatingCar && !exitingCar && !activeCarBitmap) {
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
            animatingCar = true;
        }
    }
}