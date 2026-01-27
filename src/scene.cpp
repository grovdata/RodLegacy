#include "scene.h"
#include "state.h"
#include "ui/zone.h"
#include "ui/button.h"
#include "ui/bubble.h"
#include "ui/hud.h"
#include "dialogs/dialog.h"
#include "assets.h"
#include "mouse.h"

// Forward declaration for mouse pointer update on scene change
extern Mouse mouse;

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

Scene::Scene() {
    this->isSetup = false;
    this->backgroundName = "";
    // Set backgroundAsset to an invalid/empty state
    this->backgroundAsset = {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)0};
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::~Scene() {
    // RAII handles cleanup automatically
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
    
    // Only load background if we have a valid asset or filename
    if (backgroundAsset.id != 0) {
        background = state.assets.GetBitmapAsset(backgroundAsset);
        background->SetVisible(true);
    } else {
        background = nullptr;
    }
    sceneTransition = SCENE_SOLID;
    surface = sdl2::SurfacePtr(SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                    0x00FF0000,
                                    0x0000FF00,
                                    0x000000FF,
                                    0xFF000000));
    if (backgroundName == "") {
        // Fill with black if no background
        if (!background) {
            SDL_FillRect(surface.get(), nullptr, SDL_MapRGB(surface->format, 0, 0, 0));
        }
    } else {
        sdl2::SurfacePtr img(IMG_Load(backgroundName.c_str()));
        if (img) {
            SDL_Rect destRect = {0, 0, img->w, img->h};
            SDL_BlitSurface(img.get(), nullptr, surface.get(), &destRect);
        }
    }
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
    textures.push_back(texture.get());
    
    m_backgroundRect.x = 0;
    m_backgroundRect.y = 0;
    m_backgroundRect.w = GAME_WIDTH;
    m_backgroundRect.h = GAME_HEIGHT;

    hud = std::make_unique<HUD>(palette, 20, 59);
    hud->SetVisible(true);
    hud->Clear();

    fadeSurface = sdl2::SurfacePtr(SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
    SDL_FillRect(fadeSurface.get(), &m_backgroundRect, SDL_MapRGB(fadeSurface->format, 0, 0, 0));
    fadeTexture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, fadeSurface.get()));
    SDL_SetTextureBlendMode(fadeTexture.get(), SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(fadeTexture.get(), SDL_ALPHA_OPAQUE);

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

void Scene::SetZoneEnabled(ZoneId id, bool enabled) {
    for (size_t i = 0; i < zones.size(); i++) {
        Zone& zone = zones.at(i);
        if (zone.GetId() == id) {
            zone.SetEnabled(enabled);
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
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Scene::GetZone: Zone not found: %d", id);
    // Need a static Zone object to return in case of failure
    static Zone nullZone(ZONE_NULL, const_cast<char*>(""), new SDL_Rect{0,0,0,0});
    return nullZone;
}

SDL_Texture *Scene::GetTexture() {
    return texture.get();
}

void Scene::Draw() {
    if (background) {
        background->Draw();
    }
    SDL_RenderCopy(renderer, texture.get(), NULL, &m_backgroundRect);

    if (debugMode) {
        DrawDebugZones();
        UpdateDebugInfo();
        debugOverlay->Draw();
    }
    hud->Draw();
}

void Scene::SetActiveDialog(Dialog *dialog) {
    activeDialog = dialog;
    if (activeDialog != nullptr) {
        for (auto& bubble : bubbles) {
            bubble->SetVisible(false);
        }
        activeDialog->Show();
    }
}

Dialog *Scene::GetActiveDialog() {
    return activeDialog;
}

void Scene::PushEvent(SDL_Event *event) {
    // Handle debug toggle
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F12 && debugEnabled) {
        ToggleDebugMode();
        return;
    }
    
    // If there's an active dialog, pass the event to it first
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return; // Dialog gets priority
    }
    
    // Handle mouse events for scene zones
    if (event->type == SDL_MOUSEMOTION) {
        ZoneId z = CheckMouseHover(event->motion.x, event->motion.y);
        // Could add hover effects here if needed
    }
    
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        if (SDL_GetTicks64() - lastActionTime > 200) {
            ZoneId z = CheckMouseHover(event->button.x, event->button.y);
            Action(z);
        }
    }
}

void Scene::Action(ZoneId zone) {
    lastActionTime = SDL_GetTicks64();
}

void Scene::FadeIn() {
    fadeAlpha = 255;
    fadeStartTime = SDL_GetTicks64();
    SDL_SetTextureAlphaMod(fadeTexture.get(), SDL_ALPHA_OPAQUE);
    sceneTransition = SCENE_FADE_IN;
}

void Scene::FadeOut() {
    fadeAlpha = 0;
    fadeStartTime = SDL_GetTicks64();
    SDL_SetTextureAlphaMod(fadeTexture.get(), SDL_ALPHA_TRANSPARENT);
    sceneTransition = SCENE_FADE_OUT;
}

void Scene::UpdateFade(Uint64 deltaTime) {
    if (sceneTransition == SCENE_FADE_IN || sceneTransition == SCENE_FADE_OUT) {
        Uint64 elapsed = SDL_GetTicks64() - fadeStartTime;
        
        if (elapsed >= FADE_DURATION_MS) {
            // Fade complete
            if (sceneTransition == SCENE_FADE_IN) {
                fadeAlpha = 0;
                sceneTransition = SCENE_SOLID;
            } else if (sceneTransition == SCENE_FADE_OUT) {
                fadeAlpha = 255;
                sceneTransition = SCENE_BLACK;
            }
            SDL_SetTextureAlphaMod(fadeTexture.get(), fadeAlpha);
            OnFadeComplete();
        } else {
            // Calculate fade progress (0.0 to 1.0)
            float progress = static_cast<float>(elapsed) / static_cast<float>(FADE_DURATION_MS);
            
            if (sceneTransition == SCENE_FADE_IN) {
                // Fade from opaque (255) to transparent (0)
                fadeAlpha = static_cast<Uint8>(255 * (1.0f - progress));
            } else if (sceneTransition == SCENE_FADE_OUT) {
                // Fade from transparent (0) to opaque (255)
                fadeAlpha = static_cast<Uint8>(255 * progress);
            }
            
            SDL_SetTextureAlphaMod(fadeTexture.get(), fadeAlpha);
        }
    }
}

void Scene::Update(Uint64 deltaTime) {
    UpdateFade(deltaTime);
}

void Scene::OnFadeComplete() {
    if (sceneTransition == SCENE_FADE_OUT) {
        sceneTransition = SCENE_BLACK;
        //state.MoveToScene(sceneTransitioningTo);
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_TRANSPARENT);
    } else if (sceneTransition == SCENE_FADE_IN) {
        sceneTransition = SCENE_SOLID;
        // SDL_SetTextureAlphaMod(this->m_Texture, SDL_ALPHA_OPAQUE);
        // Update mouse pointer when scene fade-in completes (only on scene change, not every frame)
        mouse.SetPointerType(GetPointerType());
    }
}

void Scene::OnSceneExit() {
    // Base implementation - can be overridden by derived classes
    // This is called when the scene is being exited via hotkeys or other means
}

void CarScene::OnSceneExit() {
    // Reset common car animation state
    carAnimationTime = 0;
    carExitAnimationTime = 0;
    waitTimer = 0;
    exitingCar = false;
    isPaused = false;
    needsNewAnimation = true;
    animatingCar = false;
    carBitmap = nullptr;  // This ensures no car is visible when re-entering
    initialDelay = true;
    
    // Call parent implementation
    Scene::OnSceneExit();
}

void Scene::UpdateDebugInfo() {
    // Base implementation - can be overridden by derived classes
    if (debugOverlay) {
        debugOverlay->Clear();
        debugOverlay->AddLine("Scene: " + std::string(typeid(*this).name()));
        debugOverlay->AddLine("Time: " + std::to_string(SDL_GetTicks64()));
    }
}

void Scene::AddGamepadDebugInfo(const Mouse& mouse) {
    SDL_Log("AddGamepadDebugInfo called - debugOverlay: %s, debugMode: %s", 
            debugOverlay ? "EXISTS" : "NULL", debugMode ? "ENABLED" : "DISABLED");
    
    if (!debugOverlay) {
        SDL_Log("No debug overlay available");
        return;
    }
    
    std::vector<std::string> gamepadLines = mouse.GetGamepadDebugLines();
    SDL_Log("Got %zu gamepad debug lines", gamepadLines.size());
    
    for (const auto& line : gamepadLines) {
        debugOverlay->AddLine(line);
        SDL_Log("Added debug line: %s", line.c_str());
    }
}

void Scene::DrawDebugZones() {
    if (!debugMode) return;
    
    // Create a yellow semi-transparent surface for zone highlighting
    sdl2::SurfacePtr zoneSurface(SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32));
    SDL_SetSurfaceBlendMode(zoneSurface.get(), SDL_BLENDMODE_BLEND);
    
    // Yellow with 50% transparency
    Uint32 zoneColor = SDL_MapRGBA(zoneSurface->format, 255, 255, 0, 128);
    
    // Draw each zone
    for (auto zone : zones) {
        SDL_Rect zoneRect = zone.GetRect(); // Assuming rect is public or has a getter
        SDL_FillRect(zoneSurface.get(), &zoneRect, zoneColor);
    }
    
    // Create texture and render
    sdl2::TexturePtr zoneTexture(SDL_CreateTextureFromSurface(renderer, zoneSurface.get()));
    SDL_SetTextureBlendMode(zoneTexture.get(), SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer, zoneTexture.get(), NULL, NULL);
    
    // RAII handles cleanup automatically
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
    SDL_Log("CarScene::StartAnimateCar - Entry point (animatingCar=%d, type=%d)", animatingCar, animType);

    currentAnimationType = animType;
    
    // Stop any existing audio and restart for this animation
    if (audioManager) {
        // Always stop first to ensure clean restart
        audioManager->StopLoop(0);
        SDL_Log("StartAnimateCar: Stopped existing audio");
        
        // Reset engine start timer
        waitingForEngineStart = false;
        engineStartTimer = 0;
        
        if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
            // Reverse out: play engine start sample FIRST, then loop after delay
            // Engine start: 31763 samples at 11025 Hz = ~2.88s duration
            SDL_Log("StartAnimateCar: Playing ENGINE_START sample for reverse");
            audioManager->PlaySound(AudioSampleType::ENGINE_START, 0.6f);
            
            // Set flag to wait for engine start to finish
            waitingForEngineStart = true;
            engineStartTimer = 0;
            
            // Pre-configure the engine loop period (will start after delay)
            audioManager->UpdateEnginePeriod(605);
            SDL_Log("StartAnimateCar: Will start ENGINE_LOOP after %.3fs", ENGINE_START_DURATION);
        } else if (animType == CarAnimationType::SINGLE_CAR_DRIVE || animType == CarAnimationType::OPPONENT_CONTINUOUS) {
            // Driving in: start at high pitch (period 320), drop to low (period 800)
            SDL_Log("StartAnimateCar: Starting ENGINE_LOOP for drive in at period 320");
            audioManager->UpdateEnginePeriod(320);  // Set period BEFORE starting (fixes high-pitch glitch)
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
        }
    }
    
    // For reverse animation with existing bitmap, just start the animation
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
        
        // Set initial position based on animation type
        if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
            // Reverse: start at idle position (car is already visible)
            SDL_Point idlePos = GetCarIdlePosition();
            carx = idlePos.x;
        } else {
            // Forward: start off-screen left
            carx = -GAME_WIDTH;
        }
        
        cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().index);
        carBitmap->Move({carx, cary});
        carAnimationTime = 0;  // Reset animation timer
        animatingCar = true;
        waitTimer = 0;
        needsNewAnimation = false;  // Reset flag
        SDL_Log("CarScene::StartAnimateCar - initialized with x=%d, y=%d, type=%d, animatingCar=%d, time=%f", 
                carx, cary, (int)animType, animatingCar, carAnimationTime);
    }
}

SDL_Point CarScene::GetCarIdlePosition() {
    return {0, 0};
}

void CarScene::DrawMaskAfterCar() {
    SDL_RenderCopy(renderer, fadeTexture.get(), NULL, &m_backgroundRect);
}

void CarScene::DrawExtra() {
    DrawMaskAfterCar();
}

void CarScene::UpdateDebugInfo() {
    if (!debugMode) return;
    Scene::UpdateDebugInfo();
}

// Helper function to get Amiga period for car animation based on progress
// Based on emulator analysis: pitch DROPS during animation (period increases)
// Car uses exponential easing, so it slows dramatically at 80%+
int16_t CarScene::GetCarAnimationPeriod(double progress, bool isDrivingIn) {
    if (!isDrivingIn) {
        // Reverse out: constant period around 605 (5916 Hz)
        return 605;
    }
    
    // Driving in: High pitch → Low pitch (period increases)
    // Match the car's exponential slowdown curve
    // Start at high pitch, drop to low as car slows
    
    if (progress < 0.6) {
        // First 60%: Stay at medium-high pitch
        return 320;  // ~11186 Hz
    } else if (progress < 0.85) {
        // 60-85%: Gradual drop as car slows
        float t = (progress - 0.6f) / 0.25f;  // 0 to 1
        return static_cast<int16_t>(320 + (t * (600 - 320)));  // 320 -> 600
    } else {
        // Last 15%: Final drop to low rumble as car settles
        float t = (progress - 0.85f) / 0.15f;  // 0 to 1
        return static_cast<int16_t>(600 + (t * (800 - 600)));  // 600 -> 800
    }
}

void CarScene::UpdateCarAnimation(double deltaTime) {
    // SDL_Log("UpdateCarAnimation: isPaused=%d, exitingCar=%d, animatingCar=%d", 
    //         isPaused, exitingCar, animatingCar);
    
    // Handle engine start delay (for reverse out)
    if (waitingForEngineStart) {
        engineStartTimer += deltaTime;
        if (engineStartTimer >= ENGINE_START_DURATION) {
            // Engine start sample finished, now start the loop
            waitingForEngineStart = false;
            if (audioManager) {
                SDL_Log("UpdateCarAnimation: Engine start finished, starting loop at period 605");
                audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
            }
        } else {
            // Still waiting for engine start sample to finish - don't move car yet
            return;
        }
    }
            
    if (!isPaused) {
        if (exitingCar) {            
            carExitAnimationTime += deltaTime;
            const double exitDuration = 3.5;  // Slowed down from 2.5
            
            double t = carExitAnimationTime / exitDuration;
            t = std::min(1.0, t);
            double easedValue = t * t * t;  // Cubic easing
            
            int startX = carx;
            int endX = GAME_WIDTH + 50;
            int newX = static_cast<int>(startX + (easedValue * (endX - startX)));
            
            carx = newX;

            float speed = (endX - startX) * (3 * t * t) / exitDuration;

            if (carBitmap) {
                carBitmap->UpdateWheelAnimation(speed * deltaTime);
                carBitmap->Move({carx, cary});
            }
            
            // Update engine sound based on animation progress (reverse out)
            if (audioManager) {
                // Reverse out: constant period 0x25D (605) = 5916 Hz
                int16_t period = GetCarAnimationPeriod(t, false);
                audioManager->UpdateEnginePeriod(period);
            }

            if (carExitAnimationTime >= exitDuration) {
                exitingCar = false;
                carBitmap = nullptr;
                carExitAnimationTime = 0;
                
                // Stop engine sound when animation completes
                if (audioManager) {
                    audioManager->StopLoop(0);  // Channel 0 is engine sound
                }
                
                OnCarExitComplete();
                
                if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
                    waitTimer = 0;
                    needsNewAnimation = true;  // Reset flag for continuous animations
                }
            }
        } else if (animatingCar) {
            // SDL_Log("UpdateCarAnimation: Handling car animation, time=%.2f", carAnimationTime);
            carAnimationTime += deltaTime;
            const double duration = 3.0;  // Slowed down from 2.0
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
            
            // Update engine sound based on animation progress (driving in)
            if (audioManager) {
                // Driving in: periods 605 -> 800 -> 128 (5916 Hz -> 4474 Hz -> 27965 Hz)
                double progress = carAnimationTime / duration;
                bool isDrivingIn = (currentAnimationType != CarAnimationType::SINGLE_CAR_REVERSE);
                int16_t period = GetCarAnimationPeriod(progress, isDrivingIn);
                audioManager->UpdateEnginePeriod(period);
            }

            if (carAnimationTime >= duration) {
                animatingCar = false;
                carAnimationTime = 0;
                
                // Stop engine sound when animation completes  
                if (audioManager) {
                    audioManager->StopLoop(0);  // Channel 0 is engine sound
                }
                
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

