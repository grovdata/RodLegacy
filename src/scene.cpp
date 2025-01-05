#include "scene.h"
#include "state.h"
#include "ui/zone.h"
#include "assets.h"
#include "state.h"

Scene::Scene(Asset backgroundAsset, StockPalette palette) {
    this->isSetup = false;
    this->backgroundAsset = backgroundAsset;
    this->palette = palette;
    //background->SetPosition(0, 0);
    //background->SetVisible(true);
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::Scene(std::string backgroundName) {
    this->isSetup = false;
    this->backgroundName = backgroundName;
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::~Scene() {
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(fadeSurface);
    SDL_DestroyTexture(fadeTexture);
    if (debugOverlay) {
        delete debugOverlay;
        debugOverlay = nullptr;
    }
}

void Scene::Init(bool initialDebugMode) {
    debugMode = initialDebugMode;
    debugEnabled = initialDebugMode;
    if (debugMode) {
        debugOverlay = new DebugOverlay(renderer, state.assets.GetDebugFont());
    }
    background = state.assets.GetBitmapAsset(backgroundAsset);
    background->SetVisible(true);
    sceneTransition = SCENE_SOLID;
    surface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                    0x00FF0000,
                                    0x0000FF00,
                                    0x000000FF,
                                    0xFF000000);
    if (backgroundName == "") {
    } else {
        SDL_Surface *img = IMG_Load(backgroundName.c_str());
        SDL_Rect destRect = {0, 0, img->w, img->h};
        SDL_BlitSurface(img, nullptr, surface, &destRect);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    textures.push_back(texture);
    
    m_backgroundRect.x = 0;
    m_backgroundRect.y = 0;
    m_backgroundRect.w = GAME_WIDTH;
    m_backgroundRect.h = GAME_HEIGHT;

    hud = new HUD(palette, 20, 59);
    hud->SetVisible(true);
    hud->Clear();

    fadeSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(fadeSurface, &m_backgroundRect, SDL_MapRGB(fadeSurface->format, 0, 0, 0));
    fadeTexture = SDL_CreateTextureFromSurface(renderer, fadeSurface);
    SDL_SetTextureBlendMode(fadeTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_OPAQUE);

    FadeIn();
}

void Scene::ToggleDebugMode() {
    if (!debugEnabled) return;  // Early return if debug wasn't enabled at startup
    
    debugMode = !debugMode;
    
    // Create overlay if it doesn't exist yet
    if (debugMode && !debugOverlay) {
        debugOverlay = new DebugOverlay(renderer, state.assets.GetMainFont());
    }
    
    // Clear overlay when toggling off
    if (!debugMode && debugOverlay) {
        debugOverlay->Clear();
    }
}

ZoneId Scene::CheckMouseHover(int x, int y) {
    for (size_t i = 0; i < zones.size(); i++) {
        Zone zone = zones.at(i);
        if (zone.IsInsideZone(x, y)) {
            hud->Clear();
            hud->SetText(zone.GetText());            
            return zone.GetId();
        }
    }
    // If no hover, clear bar
    hud->Clear();    
    return ZONE_NULL;
}

void Scene::SetZoneText(ZoneId id, char* text) {
   for (size_t i = 0; i < zones.size(); i++) {
        Zone zone = zones.at(i);
        if (zone.GetId() == id) {
            zone.SetText(text);
            return;
        }
    }
}

void Scene::SetZoneRect(ZoneId id, SDL_Rect *rect) {
   for (size_t i = 0; i < zones.size(); i++) {
        Zone& zone = zones.at(i);
        if (zone.GetId() == id) {
            zone.SetRect(rect);
            return;
        }
    }
}

Zone& Scene::GetZone(ZoneId id) {
    for (size_t i = 0; i < zones.size(); i++) {
        if (zones[i].GetId() == id) {
            return zones[i];
        }
    }
    // Need a static Zone object to return in case of failure
    static Zone nullZone(ZONE_NULL, const_cast<char*>(""), new SDL_Rect{0,0,0,0});
    return nullZone;
}

SDL_Texture *Scene::GetTexture() {
    return texture;
}

void Scene::Draw() {
    background->Draw();
    SDL_RenderCopy(renderer, texture, NULL, &m_backgroundRect);

    if (debugMode) {
        DrawDebugZones();
        UpdateDebugInfo();
        debugOverlay->Draw();
    }
    hud->Draw();
}

void Scene::PushEvent(SDL_Event *event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F12 && debugEnabled) {
        ToggleDebugMode();
        return;
    }
    ZoneId z = CheckMouseHover(event->motion.x, event->motion.y);
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        Action(z);
    }        
}

void Scene::Action(ZoneId zone) {
    printf("Zone was selected: %d \n", zone);
}

void Scene::FadeIn() {
    if (fadeTimerID != 0) {
        SDL_RemoveTimer(fadeTimerID);
    }
    fadeAlpha = 255;
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_OPAQUE);
    fadeTimerID = SDL_AddTimer(2, FadeCallback, this);
    sceneTransition = SCENE_FADE_IN;
}

void Scene::FadeOut() {
    SDL_RemoveTimer(fadeTimerID);
    SDL_SetTextureAlphaMod(fadeTexture, SDL_ALPHA_TRANSPARENT);
    fadeTimerID = SDL_AddTimer(2, FadeCallback, this);
    sceneTransition = SCENE_FADE_OUT;
}

Uint32 Scene::FadeCallback(Uint32 interval, void* param) {
    Scene* scene = static_cast<Scene*>(param);
    
    if (scene->sceneTransition == SCENE_FADE_IN) {
        scene->fadeAlpha -= 5;
        SDL_SetTextureAlphaMod(scene->fadeTexture, scene->fadeAlpha);
        
        if (scene->fadeAlpha <= 0) {
            scene->fadeAlpha = 255;  // Reset for next time
            scene->sceneTransition = SCENE_SOLID;
            scene->OnFadeComplete();
            SDL_RemoveTimer(scene->fadeTimerID);
            scene->fadeTimerID = 0;  // Clear the timer ID
            return 0;
        }
    }
    return interval;
}

void Scene::OnFadeComplete() {
    if (sceneTransition == SCENE_FADE_OUT) {
        sceneTransition = SCENE_BLACK;
        //state.MoveToScene(sceneTransitioningTo);
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_TRANSPARENT);
    } else if (sceneTransition == SCENE_FADE_IN) {
        sceneTransition = SCENE_SOLID;
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_OPAQUE);
    }
}

void Scene::UpdateDebugInfo() {
    // Base implementation - can be overridden by derived classes
    if (debugOverlay) {
        debugOverlay->Clear();
        debugOverlay->AddLine("Scene: " + std::string(typeid(*this).name()));
        debugOverlay->AddLine("Time: " + std::to_string(SDL_GetTicks64()));
    }
}

void Scene::DrawDebugZones() {
    if (!debugMode) return;
    
    // Create a yellow semi-transparent surface for zone highlighting
    SDL_Surface* zoneSurface = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(zoneSurface, SDL_BLENDMODE_BLEND);
    
    // Yellow with 50% transparency
    Uint32 zoneColor = SDL_MapRGBA(zoneSurface->format, 255, 255, 0, 128);
    
    // Draw each zone
    for (auto zone : zones) {
        SDL_Rect zoneRect = zone.GetRect(); // Assuming rect is public or has a getter
        SDL_FillRect(zoneSurface, &zoneRect, zoneColor);
    }
    
    // Create texture and render
    SDL_Texture* zoneTexture = SDL_CreateTextureFromSurface(renderer, zoneSurface);
    SDL_SetTextureBlendMode(zoneTexture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer, zoneTexture, NULL, NULL);
    
    // Clean up
    SDL_FreeSurface(zoneSurface);
    SDL_DestroyTexture(zoneTexture);
}

CarScene::CarScene(std::string backgroundName): Scene(backgroundName) {
}

CarScene::CarScene(Asset backgroundAsset): Scene(backgroundAsset) {
}

void CarScene::Init(bool debugMode) {
    Scene::Init(debugMode);
}

//void CarScene::Draw() {
//    if (!isSetup) {
//        Init();
//        isSetup = true;
//    }
//    Scene::Draw();
//}

void CarScene::StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) {
    SDL_Log("CarScene::StartAnimateCar - Entry point (animatingCar=%d)", animatingCar);
    SDL_Log("CarScene::StartAnimateCar - Type: %d", animType);

    currentAnimationType = animType;
    
    // For reverse animation, we want to use the existing bitmap
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE && carBitmap != nullptr) {
        SDL_Log("StartAnimateCar: Starting reverse animation with existing bitmap");
        carAnimationTime = 0;
        animatingCar = true;
        return;
    }
    
    // Use flag instead of null check
    if (car != nullptr && needsNewAnimation) {
        SDL_Log("CarScene::StartAnimateCar - starting new animation");
        if (carBitmap == nullptr) {
            carBitmap = new CarBitmap(car, driver, car->GetColor());
        }
        carx = -GAME_WIDTH;
        cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().index);
        carBitmap->Move({carx, cary});
        animatingCar = true;
        waitTimer = 0;
        needsNewAnimation = false;  // Reset flag
        SDL_Log("CarScene::StartAnimateCar - initialized with x=%d, y=%d, type=%d, animatingCar=%d", 
                carx, cary, (int)animType, animatingCar);
    }
}

SDL_Point CarScene::GetCarIdlePosition() {
    return {0, 0};
}

void CarScene::DrawMaskAfterCar() {
    SDL_RenderCopy(renderer, fadeTexture, NULL, &m_backgroundRect);
}

void CarScene::UpdateDebugInfo() {
    if (!debugMode) return;
    Scene::UpdateDebugInfo();
}

void CarScene::UpdateCarAnimation(double deltaTime) {
    // SDL_Log("UpdateCarAnimation: isPaused=%d, exitingCar=%d, animatingCar=%d", 
    //         isPaused, exitingCar, animatingCar);
            
    if (!isPaused) {
        if (exitingCar) {
            SDL_Log("UpdateCarAnimation: Handling exit animation");
            carExitAnimationTime += deltaTime;
            const double exitDuration = 2.5;
            
            double t = carExitAnimationTime / exitDuration;
            t = std::min(1.0, t);
            double easedValue = t * t * t;  // Cubic easing
            
            SDL_Point idlePos = GetCarIdlePosition();
            int startX = carx;
            int endX = GAME_WIDTH + 50;
            int newX = static_cast<int>(startX + (easedValue * (endX - startX)));
            
            carx = newX;

            float speed = (endX - startX) * (3 * t * t) / exitDuration;

            if (carBitmap) {
                carBitmap->UpdateWheelAnimation(speed * deltaTime);
                carBitmap->Move({carx, cary});
            }

            if (carExitAnimationTime >= exitDuration) {
                exitingCar = false;
                carBitmap = nullptr;
                carExitAnimationTime = 0;
                OnCarExitComplete();
                
                // Only start next car for OPPONENT_CONTINUOUS
                if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
                    waitTimer = 0;
                }
            }
        } else if (animatingCar) {
            // SDL_Log("UpdateCarAnimation: Handling car animation, time=%.2f", carAnimationTime);
            carAnimationTime += deltaTime;
            const double duration = 2.0;
            double easedValue = (1 - pow(2, -10 * carAnimationTime / duration));
            
            SDL_Point idlePos = GetCarIdlePosition();
            int targetX;
            
            if (currentAnimationType == CarAnimationType::SINGLE_CAR_REVERSE) {
                // SDL_Log("UpdateCarAnimation: Reverse animation");
                targetX = static_cast<int>((1.0 - easedValue) * (idlePos.x + GAME_WIDTH) - GAME_WIDTH);
            } else {
                // SDL_Log("UpdateCarAnimation: Forward animation");
                targetX = static_cast<int>((easedValue * (idlePos.x + GAME_WIDTH)) - GAME_WIDTH);
            }
            
            carx = targetX;
            // SDL_Log("UpdateCarAnimation: New carx=%d", carx);

            float speed;
            if (currentAnimationType == CarAnimationType::SINGLE_CAR_REVERSE) {
                speed = (GAME_WIDTH + 50) * (10.0/duration) * easedValue;  // Accelerate right
            } else {
                speed = (GAME_WIDTH + 50) * pow(2, -10 * carAnimationTime / duration) * (10.0/duration);  // Original forward speed
            }
            if (carBitmap) {
                carBitmap->UpdateWheelAnimation(speed * deltaTime);
                carBitmap->Move({carx, cary});
            }

            if (carAnimationTime >= duration) {
                animatingCar = false;
                carAnimationTime = 0;
                OnCarAnimationComplete();
                
                // For OPPONENT_CONTINUOUS, start exit timer
                if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
                    waitTimer = 0;
                }
            }
        }

        // Handle waiting timers for OPPONENT_CONTINUOUS only
        if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
            if (!animatingCar && !exitingCar && carBitmap) {
                waitTimer += deltaTime;
                if (waitTimer >= WAIT_BEFORE_EXIT) {
                    exitingCar = true;
                    waitTimer = 0;
                    carExitAnimationTime = 0;
                }
            }
            
            if (!animatingCar && !exitingCar && !carBitmap) {
                waitTimer += deltaTime;
                if (waitTimer >= WAIT_BEFORE_NEXT) {
                    waitTimer = 0;
                    StartAnimateCar(state.GetCurrentCar(), nullptr, currentAnimationType);
                }
            }
        }
    }
}

void CarScene::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
}

void CarScene::OnFadeComplete() {
    Scene::OnFadeComplete();
}