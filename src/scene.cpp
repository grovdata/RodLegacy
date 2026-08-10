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
    this->palette = StockPaletteAmigaDiner;
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
        debugOverlay = new DebugOverlay(renderer, state.GetAssets().GetDebugFont());
    }
    
    // Only load background if we have a valid asset or filename
    if (backgroundAsset.id != 0) {
        background = state.GetAssets().GetBitmapAsset(backgroundAsset);
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

    sceneTarget = sdl2::TexturePtr(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                     SDL_TEXTUREACCESS_TARGET,
                                                     GAME_WIDTH, GAME_HEIGHT));
    SDL_SetTextureBlendMode(sceneTarget.get(), SDL_BLENDMODE_NONE);

    FadeIn();
}

void Scene::ToggleDebugMode() {
    if (!debugEnabled) return;  // Early return if debug wasn't enabled at startup
    
    debugMode = !debugMode;
    
    // Create overlay if it doesn't exist yet
    if (debugMode && !debugOverlay) {
        debugOverlay = new DebugOverlay(renderer, state.GetAssets().GetMainFont());
    }
    
    // Clear overlay when toggling off
    if (!debugMode && debugOverlay) {
        debugLines.clear();
    }
}

Zone* Scene::FindZone(int x, int y) {
    for (auto& zone : zones) {
        if (zone.IsInsideZone(x, y)) {
            hud->SetText(zone.GetText().c_str());
            return &zone;
        }
    }
    hud->Clear();
    return nullptr;
}

void Scene::SetZoneText(const std::string& id, const std::string& text) {
    for (auto& zone : zones) {
        if (zone.GetId() == id) {
            zone.SetText(text);
            return;
        }
    }
}

void Scene::SetZoneRect(const std::string& id, SDL_Rect rect) {
    for (auto& zone : zones) {
        if (zone.GetId() == id) {
            zone.SetRect(rect);
            return;
        }
    }
}

void Scene::SetZoneEnabled(const std::string& id, bool enabled) {
    for (auto& zone : zones) {
        if (zone.GetId() == id) {
            zone.SetEnabled(enabled);
            return;
        }
    }
}

Zone& Scene::GetZone(const std::string& id) {
    for (auto& zone : zones) {
        if (zone.GetId() == id) {
            return zone;
        }
    }
    LOG_WARN("Scene::GetZone: Zone not found: %s", id.c_str());
    static Zone nullZone("", "", SDL_Rect{0, 0, 0, 0});
    return nullZone;
}

SDL_Texture *Scene::GetTexture() {
    return texture.get();
}

void Scene::DrawScene() {
    if (background) {
        background->Draw();
    }
    SDL_RenderCopy(renderer, texture.get(), NULL, &m_backgroundRect);

    if (debugMode) {
        // Zone highlighting stays in the game view; the text goes to the debug
        // UI when that is built, and to the legacy overlay when it is not.
        DrawDebugZones();
        DrawLegacyDebugOverlay();
    }
    hud->Draw();
}

void Scene::Draw() {
    // Restore whatever target was current on entry rather than assuming the
    // backbuffer: the caller may be rendering the game into an offscreen
    // texture, and resetting to nullptr would send the scene past it.
    SDL_Texture* callerTarget = SDL_GetRenderTarget(renderer);

    SDL_SetRenderTarget(renderer, sceneTarget.get());
    SDL_RenderClear(renderer);

    DrawScene();
    DrawExtra();

    SDL_SetRenderTarget(renderer, callerTarget);

    Uint8 c = 255 - fadeAlpha;
    SDL_SetTextureColorMod(sceneTarget.get(), c, c, c);
    SDL_RenderCopy(renderer, sceneTarget.get(), nullptr, nullptr);
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

bool Scene::PushEvent(SDL_Event *event) {
    // Handle debug toggle
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F12 && debugEnabled) {
        ToggleDebugMode();
        return true;
    }
    
    // If there's an active dialog, pass the event to it first
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return true;
    }
    
    // Handle mouse events for scene zones
    if (event->type == SDL_MOUSEMOTION) {
        FindZone(event->motion.x, event->motion.y);
        return false;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        Uint32 t = event->common.timestamp;
        if (lastActionTimestamp == 0 || static_cast<Uint32>(t - lastActionTimestamp) >= 200) {
            Zone* z = FindZone(event->button.x, event->button.y);
            lastActionTimestamp = t;
            if (z && z->onClick) z->onClick();
            return true;
        }
        return false;
    }
    return false;
}

void Scene::FadeIn() {
    fadeAlpha = 255;
    fadeStartTime = SDL_GetTicks64();
    sceneTransition = SCENE_FADE_IN;
    // Scene entry owns cursor visibility: each scene declares whether it wants
    // the pointer, so the next scene entered always sets it correctly.
    mouse.SetVisible(WantsPointer());
}

void Scene::FadeOut() {
    fadeAlpha = 0;
    fadeStartTime = SDL_GetTicks64();
    sceneTransition = SCENE_FADE_OUT;
}

void Scene::UpdateFade(Uint64) {
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
            OnFadeComplete();
        } else {
            // Calculate fade progress (0.0 to 1.0)
            float progress = static_cast<float>(elapsed) / static_cast<float>(FADE_DURATION_MS);

            // Quantize to 15 steps to match Amiga palette nibble stepping
            Uint8 step;
            if (sceneTransition == SCENE_FADE_IN) {
                step = static_cast<Uint8>((1.0f - progress) * 15);
            } else {
                step = static_cast<Uint8>(progress * 15);
            }
            fadeAlpha = step * 17;
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
    debugLines.clear();
    debugRows.clear();
    AddDebugValue("scene", typeid(*this).name());
    AddDebugValue("zones", std::to_string(zones.size()));
    AddDebugValue("buttons", std::to_string(buttons.size()));
    AddDebugValue("bubbles", std::to_string(bubbles.size()));
    AddDebugValue("fade alpha", std::to_string(static_cast<int>(fadeAlpha)));
}

void Scene::DrawLegacyDebugOverlay() {
#ifndef RODLEGACY_DEBUG_UI
    if (!debugMode || !debugOverlay) {
        return;
    }
    UpdateDebugInfo();
    debugOverlay->Clear();
    for (const DebugRow& row : debugRows) {
        debugOverlay->AddLine(row.key + ": " + row.value);
    }
    for (const std::string& line : debugLines) {
        debugOverlay->AddLine(line);
    }
    debugOverlay->Draw();
#endif
}

std::vector<DebugPanel> Scene::GetDebugPanels() {
    UpdateDebugInfo();
    DebugPanel panel{"Scene", debugLines};
    panel.rows = debugRows;
    panel.toggles.push_back({"Show zone overlays", &state.GetAssets().GetDebugShowZoneOverlaysRef()});
    return { panel };
}

void Scene::AddGamepadDebugInfo(const Mouse& mouse) {
    for (const std::string& line : mouse.GetGamepadDebugLines()) {
        AddDebugLine(line);
    }
}

void Scene::DrawDebugZones() {
    if (!debugMode || !state.GetAssets().GetDebugShowZoneOverlaysRef()) return;
    
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

    currentAnimationType = animType;
    
    // Stop any existing audio and restart for this animation
    if (audioManager) {
        // Always stop first to ensure clean restart
        audioManager->StopLoop(0);
        lastAnimationEnginePeriod = -1;
        
        // Reset engine start timer
        waitingForEngineStart = false;
        engineStartTimer = 0;
        
        if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
            // Reverse out: play engine start sample FIRST, then loop after delay
            // Engine start: 31763 samples at 11025 Hz = ~2.88s duration
            audioManager->PlaySound(AudioSampleType::ENGINE_START, 0.6f);
            
            // Set flag to wait for engine start to finish
            waitingForEngineStart = true;
            engineStartTimer = 0;
            
            // Pre-configure the engine loop period (will start after delay)
            audioManager->UpdateEnginePeriod(605);
            lastAnimationEnginePeriod = 605;
        } else if (animType == CarAnimationType::SINGLE_CAR_DRIVE || animType == CarAnimationType::OPPONENT_CONTINUOUS) {
            // Driving in: mostly constant low pitch, then deeper drop near full stop.
            audioManager->UpdateEnginePeriod(605);
            lastAnimationEnginePeriod = 605;
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
        }
    }
    
    // For reverse animation with existing bitmap, just start the animation
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE && carBitmap != nullptr) {
        carAnimationTime = 0;
        carxFloat = carx;
        animatingCar = true;
        return;
    }
    
    // Use flag instead of null check
    if (car != nullptr && needsNewAnimation) {
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
        carxFloat = carx;
        
        cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().index);
        carBitmap->Move({carx, cary});
        carAnimationTime = 0;  // Reset animation timer
        animatingCar = true;
        waitTimer = 0;
        needsNewAnimation = false;  // Reset flag
    }
}

SDL_Point CarScene::GetCarIdlePosition() {
    return {0, 0};
}

void CarScene::DrawMaskAfterCar() {
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
    
    // Driving in: keep engine tone constant, then one final low drop near stop.
    if (progress < 0.92) {
        return 605;
    }
    return 800;
}

void CarScene::UpdateCarAnimation(double deltaTime) {
    // LOG_INFO("UpdateCarAnimation: isPaused=%d, exitingCar=%d, animatingCar=%d", 
    //         isPaused, exitingCar, animatingCar);
    
    // Handle engine start delay (for reverse out)
    if (waitingForEngineStart) {
        engineStartTimer += deltaTime;
        if (engineStartTimer >= ENGINE_START_DURATION) {
            // Engine start sample finished, now start the loop
            waitingForEngineStart = false;
            if (audioManager) {
                LOG_INFO("UpdateCarAnimation: Engine start finished, starting loop at period 605");
                audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
            }
        } else {
            // Still waiting for engine start sample to finish - don't move car yet
            return;
        }
    }
            
    if (!isPaused) {
        if (exitingCar) {
            // Exit: 3-phase acceleration matching DOS port (2->4->8 px/step at 35Hz)
            SDL_Point idlePos = GetCarIdlePosition();
            double exitTotal = GAME_WIDTH + idlePos.x;
            double exitProgress = (exitTotal > 0)
                ? std::clamp((carxFloat - idlePos.x) / exitTotal, 0.0, 1.0)
                : 0.0;
            double exitSpeed;
            if (exitProgress < 0.20) {
                exitSpeed = 70.0;
            } else if (exitProgress < 0.75) {
                exitSpeed = 140.0;
            } else {
                exitSpeed = 280.0;
            }
            carxFloat += exitSpeed * deltaTime;
            carx = static_cast<int>(carxFloat);

            if (carBitmap) {
                carBitmap->UpdateWheelAnimation(exitSpeed * deltaTime);
                carBitmap->Move({carx, cary});
            }

            if (carx >= GAME_WIDTH + 50) {
                exitingCar = false;
                carBitmap = nullptr;
                carExitAnimationTime = 0;

                if (audioManager)
                    audioManager->StopLoop(0);

                OnCarExitComplete();

                if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
                    waitTimer = 0;
                    needsNewAnimation = true;
                }
            }
        } else if (animatingCar) {
            SDL_Point idlePos = GetCarIdlePosition();
            bool isDrivingIn = (currentAnimationType != CarAnimationType::SINGLE_CAR_REVERSE);
            bool done;

            if (isDrivingIn) {
                // Drive-in: 3-phase deceleration matching DOS port (8->4->2 px/step at 35Hz)
                // Phase 0 (fast, 280 px/s): first ~25% of distance
                // Phase 1 (medium, 140 px/s): middle ~55%
                // Phase 2 (slow, 70 px/s): final ~20%
                double totalDist = idlePos.x + GAME_WIDTH;
                double progress = (totalDist > 0)
                    ? std::clamp((carxFloat + GAME_WIDTH) / totalDist, 0.0, 1.0)
                    : 0.0;
                double driveSpeed;
                if (progress < 0.25) {
                    driveSpeed = 280.0;
                } else if (progress < 0.80) {
                    driveSpeed = 140.0;
                } else {
                    driveSpeed = 70.0;
                }
                carxFloat += driveSpeed * deltaTime;
                carx = static_cast<int>(carxFloat);
                done = (carx >= idlePos.x);
                if (done) carx = idlePos.x;

                if (carBitmap) {
                    carBitmap->UpdateWheelAnimation(driveSpeed * deltaTime);
                    carBitmap->Move({carx, cary});
                }
            } else {
                // SINGLE_CAR_REVERSE: 3-phase acceleration (2->4->8 px/step at 35Hz),
                // backing out to the left along the same path the car drove in on.
                double totalDist = idlePos.x + GAME_WIDTH;
                double progress = (totalDist > 0)
                    ? std::clamp((idlePos.x - carxFloat) / totalDist, 0.0, 1.0)
                    : 0.0;
                double reverseSpeed;
                if (progress < 0.20) {
                    reverseSpeed = 70.0;
                } else if (progress < 0.75) {
                    reverseSpeed = 140.0;
                } else {
                    reverseSpeed = 280.0;
                }
                carxFloat -= reverseSpeed * deltaTime;
                carx = static_cast<int>(carxFloat);
                done = (carx <= -GAME_WIDTH);

                if (carBitmap) {
                    carBitmap->UpdateWheelAnimation(-reverseSpeed * deltaTime);
                    carBitmap->Move({carx, cary});
                }
            }

            if (audioManager) {
                if (isDrivingIn) {
                    double audioDist = idlePos.x + GAME_WIDTH;
                    double audioProgress = (audioDist > 0)
                        ? std::clamp((carx + GAME_WIDTH) / audioDist, 0.0, 1.0)
                        : 0.0;
                    const int16_t targetPeriod = GetCarAnimationPeriod(audioProgress, true);
                    if (targetPeriod != lastAnimationEnginePeriod) {
                        audioManager->UpdateEnginePeriod(targetPeriod);
                        lastAnimationEnginePeriod = targetPeriod;
                    }
                }
            }

            if (done) {
                animatingCar = false;
                carAnimationTime = 0;

                if (audioManager)
                    audioManager->StopLoop(0);

                OnCarAnimationComplete();

                if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS)
                    waitTimer = 0;
            }
        }

        // Handle waiting timers for OPPONENT_CONTINUOUS only
        if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS) {
            if (!animatingCar && !exitingCar && carBitmap) {
                if (!CanStartCarExit()) {
                    waitTimer = 0;
                } else {
                    waitTimer += deltaTime;
                    if (waitTimer >= WAIT_BEFORE_EXIT) {
                        exitingCar = true;
                        waitTimer = 0;
                        carExitAnimationTime = 0;
                    }
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

void CarScene::TriggerCarExit() {
    if (!exitingCar && carBitmap) {
        exitingCar = true;
        waitTimer = 0;
        carExitAnimationTime = 0;
        carxFloat = carx;
    }
}

bool CarScene::PushEvent(SDL_Event *event) {
    return Scene::PushEvent(event);
}

void CarScene::OnFadeComplete() {
    Scene::OnFadeComplete();
}

