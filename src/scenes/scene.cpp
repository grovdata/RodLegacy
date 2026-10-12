#include "scenes/scene.h"
#include "core/state.h"
#include "ui/zone.h"
#include "ui/button.h"
#include "ui/bubble.h"
#include "ui/hud.h"
#include "dialogs/dialog.h"
#include "core/assets.h"
#include "core/mouse.h"
#include "core/utils.h"
#include "audio/engine_sound.h"

#include <random>

extern Mouse mouse;

Scene::Scene(Asset backgroundAsset, StockPalette palette) {
    this->isSetup = false;
    this->backgroundAsset = backgroundAsset;
    this->palette = palette;
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
    this->backgroundAsset = {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)0};
    this->palette = StockPaletteAmigaDiner;
    debugOverlay = nullptr;
    debugMode = false;
}

Scene::~Scene() {
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

    // Status bar pens are nearest-colour matches of 0x0C71 (text) and 0x0333 (background) in the scene's palette,
    // EHB included: PAL1 gives 59 and 20, PAL_GAS 31 and 20.
    if (backgroundAsset.game == SrGame::SR2) {
        // Street Rod 2's bar is pen 3 on pen 20 of the screen's own palette.
        BitmapParser parser;
        const Palette rgb = parser.PreparePalette(state.GetAssets().GetAssetPalette(backgroundAsset), true);
        hud = std::make_unique<HUD>(rgb, 20, 3);
    } else {
        const int fontColorIndex = (palette == StockPaletteAmigaGasStation) ? 31 : 59;
        hud = std::make_unique<HUD>(palette, 20, fontColorIndex);
    }
    hud->SetVisible(true);
    hud->Clear();

    sceneTarget = sdl2::TexturePtr(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                     SDL_TEXTUREACCESS_TARGET,
                                                     GAME_WIDTH, GAME_HEIGHT));
    SDL_SetTextureBlendMode(sceneTarget.get(), SDL_BLENDMODE_NONE);

    FadeIn();
}

void Scene::ToggleDebugMode() {
    if (!debugEnabled) return;
    
    debugMode = !debugMode;
    
    if (debugMode && !debugOverlay) {
        debugOverlay = new DebugOverlay(renderer, state.GetAssets().GetMainFont());
    }
    
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

Zone* Scene::ZoneById(const std::string& id) {
    for (auto& zone : zones) {
        if (zone.GetId() == id) {
            return &zone;
        }
    }
    return nullptr;
}

Zone& Scene::GetZone(const std::string& id) {
    if (Zone* zone = ZoneById(id)) {
        return *zone;
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
    // Restore the caller's target: the game may be rendering into an offscreen texture.
    SDL_Texture* callerTarget = SDL_GetRenderTarget(renderer);

    SDL_SetRenderTarget(renderer, sceneTarget.get());
    SDL_RenderClear(renderer);

    DrawScene();
    DrawExtra();

    SDL_SetRenderTarget(renderer, callerTarget);

    SDL_RenderCopy(renderer, sceneTarget.get(), nullptr, nullptr);
    DrawFadeClamp();
}

// The Amiga fade clamps each 4-bit channel to a falling ceiling, a per-channel minimum, so it is drawn as a
// full-screen grey with a minimum blend.
void Scene::DrawFadeClamp() {
    if (fadeLevel >= FADE_LEVELS) {
        return;
    }

    static const SDL_BlendMode minBlend = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM,
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MINIMUM);

    // Palettes are expanded as nibble << 4, so level 15 is 240, not 255.
    Uint8 c = static_cast<Uint8>(fadeLevel * 16);
    if (SDL_SetRenderDrawBlendMode(renderer, minBlend) == 0) {
        SDL_SetRenderDrawColor(renderer, c, c, c, 255);
    } else {
        // ponytail: no minimum blending on this renderer, fall back to the old
        // proportional wash. Drop it if every backend we ship supports MINIMUM.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255 - c));
    }
    SDL_RenderFillRect(renderer, nullptr);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F12 && debugEnabled) {
        ToggleDebugMode();
        return true;
    }
    
    if (activeDialog && activeDialog->IsVisible()) {
        return activeDialog->HandleEvent(event);
    }
    
    if (event->type == SDL_MOUSEMOTION) {
        FindZone(event->motion.x, event->motion.y);
        return false;
    }

    if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) &&
        event->button.button == SDL_BUTTON_LEFT) {
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
    fadeLevel = 0;
    fadeStartTime = GameTicks();
    sceneTransition = SCENE_FADE_IN;
    // Pointer type and visibility are restored together on entry, or the previous cursor lingers through the fade.
    mouse.SetVisible(WantsPointer());
    mouse.SetPointerType(GetPointerType());
}

void Scene::SetPointer(SrLibAmigaSpriteAsset type) {
    currentPointer = type;
    mouse.SetPointerType(type);
}

void Scene::FadeOut() {
    fadeLevel = FADE_LEVELS;
    fadeStartTime = GameTicks();
    sceneTransition = SCENE_FADE_OUT;
}

void Scene::UpdateFade(Uint64) {
    if (sceneTransition != SCENE_FADE_IN && sceneTransition != SCENE_FADE_OUT) {
        return;
    }

    // One level per vblank, off wall clock.
    Uint64 step = (GameTicks() - fadeStartTime) / FADE_STEP_MS;

    if (sceneTransition == SCENE_FADE_IN) {
        fadeLevel = static_cast<Uint8>(std::min<Uint64>(step, FADE_LEVELS));
        if (step >= FADE_IN_STEPS) {
            sceneTransition = SCENE_SOLID;
            OnFadeComplete();
        }
    } else {
        fadeLevel = step >= FADE_LEVELS ? 0 : static_cast<Uint8>(FADE_LEVELS - step);
        if (step >= FADE_OUT_STEPS) {
            sceneTransition = SCENE_BLACK;
            OnFadeComplete();
        }
    }
}

void Scene::Update(Uint64 deltaTime) {
    UpdateFade(deltaTime);
}

void Scene::OnFadeComplete() {
    if (sceneTransition == SCENE_FADE_OUT) {
        sceneTransition = SCENE_BLACK;
    } else if (sceneTransition == SCENE_FADE_IN) {
        sceneTransition = SCENE_SOLID;
    }
}

void Scene::OnSceneEnter() {
}

void Scene::OnSceneExit() {
}

void CarScene::OnSceneExit() {
    carAnimationTime = 0;
    carExitAnimationTime = 0;
    waitTimer = 0;
    exitingCar = false;
    isPaused = false;
    needsNewAnimation = true;
    animatingCar = false;
    carBitmap = nullptr;
    initialDelay = true;
    if (audioManager) {
        audioManager->StopLoop(0);
    }

    Scene::OnSceneExit();
}

void Scene::UpdateDebugInfo() {
    debugLines.clear();
    debugRows.clear();
    AddDebugValue("scene", typeid(*this).name());
    AddDebugValue("zones", std::to_string(zones.size()));
    AddDebugValue("buttons", std::to_string(buttons.size()));
    AddDebugValue("bubbles", std::to_string(bubbles.size()));
    AddDebugValue("fade level", std::to_string(static_cast<int>(fadeLevel)));
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
    
    sdl2::SurfacePtr zoneSurface(SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32));
    SDL_SetSurfaceBlendMode(zoneSurface.get(), SDL_BLENDMODE_BLEND);
    
    Uint32 zoneColor = SDL_MapRGBA(zoneSurface->format, 255, 255, 0, 128);
    
    for (auto zone : zones) {
        SDL_Rect zoneRect = zone.GetRect();
        SDL_FillRect(zoneSurface.get(), &zoneRect, zoneColor);
    }
    
    sdl2::TexturePtr zoneTexture(SDL_CreateTextureFromSurface(renderer, zoneSurface.get()));
    SDL_SetTextureBlendMode(zoneTexture.get(), SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(renderer, zoneTexture.get(), NULL, NULL);
}

CarScene::CarScene(std::string backgroundName): Scene(backgroundName) {
}

CarScene::CarScene(Asset backgroundAsset, StockPalette palette): Scene(backgroundAsset, palette) {
}

void CarScene::Init(bool debugMode) {
    Scene::Init(debugMode);
}

void CarScene::StartAnimateCar(CarPtr car, Driver* driver, CarAnimationType animType) {

    currentAnimationType = animType;
    
    if (audioManager) {
        audioManager->StopLoop(0);
        audioManager->SetMufflerRemoved(car != nullptr && car->IsMufflerRemoved());
        lastAnimationEnginePeriod = -1;
        engineBase = 600 + std::uniform_int_distribution<int>(0, 9)(RandomEngine());

        waitingForEngineStart = false;
        engineStartTimer = 0;

        if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
            // Engine start: ~2.88s.
            audioManager->PlaySound(AudioSampleType::ENGINE_START, 0.6f);

            waitingForEngineStart = true;
            engineStartTimer = 0;

            ApplyAnimationEngine(false, false);
        } else if (animType == CarAnimationType::SINGLE_CAR_DRIVE || animType == CarAnimationType::OPPONENT_CONTINUOUS) {
            ApplyAnimationEngine(true, false);
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
        }
    }
    
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE && carBitmap != nullptr) {
        carAnimationTime = 0;
        carxFloat = carx;
        stepAccumulator = 0.0;
        animatingCar = true;
        return;
    }
    
    if (car && needsNewAnimation) {
        if (carBitmap == nullptr) {
            carBitmap = new CarBitmap(car, driver, car->GetColor());
        }
        
        if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
            SDL_Point idlePos = GetCarIdlePosition();
            carx = idlePos.x;
        } else {
            carx = -GAME_WIDTH;
        }
        carxFloat = carx;
        stepAccumulator = 0.0;
        
        cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().height,
                                          state.GetAssets().GetGame());
        carBitmap->Move({carx, cary});
        carAnimationTime = 0;
        animatingCar = true;
        waitTimer = 0;
        needsNewAnimation = false;
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

void CarScene::ApplyAnimationEngine(bool drivingIn, bool secondPhase) {
    const EngineSound::Note note = EngineSound::AnimationNote(engineBase, drivingIn, secondPhase);
    if (!audioManager || note.period == lastAnimationEnginePeriod) return;
    audioManager->UpdateEnginePeriod(note.period, note.paulaVolume);
    lastAnimationEnginePeriod = note.period;
}

// Moves a whole number of pixels per tick of the original's clock; usually 0 this frame.
double CarScene::StepCarX(double deltaTime, double pxPerSecond) {
    const double tick = 1.0 / SCENE_STEP_HZ;
    const double step = std::round(pxPerSecond * tick);
    stepAccumulator += deltaTime;
    double moved = 0.0;
    while (stepAccumulator >= tick) {
        stepAccumulator -= tick;
        moved += step;
    }
    return moved;
}

void CarScene::UpdateCarAnimation(double deltaTime) {
    if (waitingForEngineStart) {
        engineStartTimer += deltaTime;
        if (engineStartTimer >= ENGINE_START_DURATION) {
            waitingForEngineStart = false;
            if (audioManager) {
                audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
            }
        } else {
            return;
        }
    }
            
    if (!isPaused) {
        if (exitingCar) {
            // 2->4->8 px/step at 35Hz.
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
            carxFloat += StepCarX(deltaTime, exitSpeed);
            carx = static_cast<int>(carxFloat);
            ApplyAnimationEngine(false, exitProgress >= 0.20);

            if (carBitmap) {
                carBitmap->UpdateWheelRotation(carx);
                carBitmap->Move({carx, cary});
            }
            if (audioManager) audioManager->UpdateEngineDistance(carx - idlePos.x);

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
                // 8->4->2 px/step at 35Hz: fast ~25% of the distance, medium ~55%, slow ~20%.
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
                carxFloat += StepCarX(deltaTime, driveSpeed);
                carx = static_cast<int>(carxFloat);
                ApplyAnimationEngine(true, progress >= 0.80);
                done = (carx >= idlePos.x);
                if (done) carx = idlePos.x;

                if (carBitmap) {
                    carBitmap->UpdateWheelRotation(carx);
                    carBitmap->Move({carx, cary});
                }
            } else {
                // 2->4->8 px/step, backing out along the drive-in path.
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
                carxFloat -= StepCarX(deltaTime, reverseSpeed);
                carx = static_cast<int>(carxFloat);
                done = (carx <= -GAME_WIDTH);

                if (carBitmap) {
                    carBitmap->UpdateWheelRotation(carx);
                    carBitmap->Move({carx, cary});
                }
            }

            if (audioManager) audioManager->UpdateEngineDistance(carx - idlePos.x);

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
                    StartAnimateCar(state.GetCurrentCarPtr(), nullptr, currentAnimationType);
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
        stepAccumulator = 0.0;
    }
}

bool CarScene::PushEvent(SDL_Event *event) {
    return Scene::PushEvent(event);
}

void CarScene::OnFadeComplete() {
    Scene::OnFadeComplete();
}

