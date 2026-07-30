#include "log.h"
#include "gas_station.h"
#include "state.h"
#include "parsers/libp.h"
#include "animated_bitmap.h"
#include "scene_layout.h"
#include "embedded_resources.h"
#include <memory>
double carAnimationTimeGasStation = 0;

GasStation::GasStation() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_SCREEN}) {
    zones = SceneLayout::Load(embedded_gas_station_toml);

    GetZone("pump").onClick = [this] {
        if (carBitmap && sceneState == GAS_STATION_IDLE) {
            sceneState = GAS_STATION_HANDLEOFF;
            carBitmap->SetGasCapRemoved(true);
            carBitmap->SetPumpingHandleVisible(true);
            auto pumpNoHandle = dynamic_cast<Bitmap*>(GetDrawable("pump_no_handle"));
            if (pumpNoHandle) pumpNoHandle->SetVisible(true);
        }
        currentPointer = SrLibAmigaSpriteAssetMousePointerPump;
    };
    GetZone("rag").onClick = [this] {
        currentPointer = SrLibAmigaSpriteAssetMousePointerRag;
    };

    carx = -GAME_WIDTH;
    cary = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().index);

    //hud = new HUD(StockPalette::GUS_GAS_STATION, 20, 18);
    // Hud color text index 18
}

GasStation::~GasStation() {
    // Smart pointers will automatically clean up
}

void GasStation::Init(bool initialDebugMode) {
    Scene::Init(initialDebugMode);

    // Set audio manager for engine sound
    SetAudioManager(state.GetAssets().GetAudioManager());

    // Create a mask for gas pump
    auto maskBitmap = state.GetAssets().GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, GUS_GAS_MASK});
    maskBitmap->SetPosition(0, 107);
    maskedBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 312, 69, 32, SDL_PIXELFORMAT_RGBA8888));
    background->CreateMask(maskBitmap->GetSurface(), maskedBackground.get(), {0, 107, 312, 69});
    maskedBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedBackground.get()));
    SDL_SetTextureBlendMode(maskedBackgroundTexture.get(), SDL_BLENDMODE_BLEND);
    
    // Create road mask to hide car wheels below y=175 (similar to diner scene)
    roadMaskSurface.reset(SDL_CreateRGBSurface(0, 320, 15, 32, 0, 0, 0, 0));
    if (roadMaskSurface) {
        SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
        roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
        SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
        LOG_INFO("Created road mask: src={%d,%d,%d,%d} dst={%d,%d,%d,%d}", 
                roadMaskSrcRect.x, roadMaskSrcRect.y, roadMaskSrcRect.w, roadMaskSrcRect.h,
                roadMaskDestRect.x, roadMaskDestRect.y, roadMaskDestRect.w, roadMaskDestRect.h);
    } else {
        LOG_INFO("Failed to create road mask surface");
    }

    // Add pump no handle to container (asset-owned, non-owning ref)
    auto pumpNoHandle = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_PUMP2});
    pumpNoHandle->SetPosition(56, 114);
    pumpNoHandle->SetVisible(true);
    AddDrawable("pump_no_handle", pumpNoHandle);

    // Add attendant as AnimatedBitmap to container (scene-owned)
    Bitmap* attendantSheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, GUS_GAS_ATTENDANT});
    SDL_Rect attendantRect = {200, 90, 47, 47};
    attendantAnim = std::make_unique<AnimatedBitmap>(attendantSheet, attendantRect);
    attendantAnim->SetAnimationFrames(47, 47, 4, 1); // 2x2 grid
    attendantAnim->SetSpeed(0.5f); // 0.5s per frame
    attendantAnim->SetLoop(true);
    attendantAnim->Play();
    AddDrawable("attendant", attendantAnim.get());
    
    initStartTime = SDL_GetTicks64();
}


SDL_Point GasStation::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GAS_STATION);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().index);
    return {x, y};
}

void GasStation::DrawScene() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    // Update attendant animation
    auto attendant = dynamic_cast<AnimatedBitmap*>(GetDrawable("attendant"));
    if (attendant) {
        attendant->Update(deltaTime);
    }

    Scene::DrawScene();

    // Wait 2s
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
        return;
    }

    // Draw all drawables in the container
    DrawAll();
    
    // Draw car bitmap separately since it's not in the container
    if (carBitmap != nullptr) {
        carBitmap->Draw();
    }
    
    DrawDebugZones();
}

void GasStation::DrawMaskAfterCar() {
    // Draw the gas pump mask
    SDL_Rect rect = {0, 107, 312, 69};
    SDL_RenderCopy(renderer, maskedBackgroundTexture.get(), NULL, &rect);
    
    if (roadMaskSurface && roadMaskTexture) {
        SDL_BlitSurface(surface.get(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
        roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
        SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer, roadMaskTexture.get(), NULL, &roadMaskSrcRect);
    }
    
    // Call parent implementation for any additional mask effects
    CarScene::DrawMaskAfterCar();
}

void GasStation::UpdateCarAnimation(double deltaTime) {
    CarScene::UpdateCarAnimation(deltaTime);
    if (!animatingCar) {
        return;
    }

    // Move car bitmap to current position
    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
    }

    CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
    CarClickBoxes_t clickBoxes = metadata.clickBoxes;
    
    SDL_Rect gasCapRect = {carx + clickBoxes.clickBoxGasCap.x, cary + clickBoxes.clickBoxGasCap.y, clickBoxes.clickBoxGasCap.width, clickBoxes.clickBoxGasCap.height};
    SetZoneRect("gas_cap", gasCapRect);

    SDL_Rect rearWindowRect = {metadata.offsets.offsetXRearTire + clickBoxes.clickBoxRearWindow.x, cary + clickBoxes.clickBoxRearWindow.y, clickBoxes.clickBoxRearWindow.width, clickBoxes.clickBoxRearWindow.height};
    SetZoneRect("car_window_rear", rearWindowRect);

    SDL_Rect frontWindowRect = {metadata.offsets.offsetXRearTire + clickBoxes.clickBoxFrontWindow.x, cary + clickBoxes.clickBoxFrontWindow.y, clickBoxes.clickBoxFrontWindow.width, clickBoxes.clickBoxFrontWindow.height};
    SetZoneRect("car_window_front", frontWindowRect);
}

void GasStation::UpdateDebugInfo() {
    if (!debugMode) return;
    
    CarScene::UpdateDebugInfo();
        
    if (debugOverlay) {
        debugLines.clear();
        
        AddDebugLine("Car X: " + std::to_string(carx) + " Y: " + std::to_string(cary));        

        if (carBitmap != nullptr) {
            CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
            CarClickBoxes_t clickBoxes = metadata.clickBoxes;
            
            AddDebugLine("cap x=" + std::to_string(metadata.offsets.offsetXGasCap) + 
                                " y=" + std::to_string(metadata.offsets.offsetYGasCap));

            AddDebugLine("rear: x=" + std::to_string(clickBoxes.clickBoxRearWindow.x) + 
                                " y=" + std::to_string(clickBoxes.clickBoxRearWindow.y) +
                                " w=" + std::to_string(clickBoxes.clickBoxRearWindow.width) +
                                " h=" + std::to_string(clickBoxes.clickBoxRearWindow.height));

            AddDebugLine("front: x=" + std::to_string(clickBoxes.clickBoxFrontWindow.x) + 
                                " y=" + std::to_string(clickBoxes.clickBoxFrontWindow.y) +
                                " w=" + std::to_string(clickBoxes.clickBoxFrontWindow.width) +
                                " h=" + std::to_string(clickBoxes.clickBoxFrontWindow.height));
        }
        
    }
}

bool GasStation::PushEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (carBitmap) {
            carBitmap->SetGasCapRemoved(false);
            carBitmap->SetPumpingHandleVisible(false);
            auto pumpNoHandle = dynamic_cast<Bitmap*>(GetDrawable("pump_no_handle"));
            if (pumpNoHandle) pumpNoHandle->SetVisible(false);
        }
        currentPointer = SrLibAmigaSpriteAssetMousePointerHand;
    }
    bool consumed = Scene::PushEvent(event);
    if (!debugMode) return consumed;

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_TAB:
                break;
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_UP:
            case SDLK_DOWN: {
                Zone& rearWindow = GetZone("car_window_rear");
                SDL_Rect targetRect = rearWindow.GetRect();
                int delta = (event->key.keysym.mod & KMOD_SHIFT) ? 5 : 1;
                switch (event->key.keysym.sym) {
                    case SDLK_LEFT:
                        targetRect.x -= delta;
                        break;
                    case SDLK_RIGHT:
                        targetRect.x += delta;
                        break;
                    case SDLK_UP:
                        targetRect.y -= delta;
                        break;
                    case SDLK_DOWN:
                        targetRect.y += delta;
                        break;
                }
                rearWindow.SetRect(targetRect);
                return true;
            }
        }
    }
    return consumed;
}

void GasStation::StartAnimateCar(Car *car, Driver*, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;

    if (car != nullptr && carBitmap == nullptr) {
        carBitmap = GetOrCreateCurrentCarBitmap();
        if (carBitmap) {
            carx = -GAME_WIDTH;
            cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().index);
            animatingCar = true;
            currentAnimationType = animType;
        }
    }
}

void GasStation::OnCarAnimationComplete() {
    CarScene::OnCarAnimationComplete();
    carBitmap->SetGasCapRemoved(true);
    carBitmap->SetPumpingHandleVisible(true);
}


CarBitmap* GasStation::GetOrCreateCurrentCarBitmap() {
    Car* cur = state.GetCurrentCar();
    if (currentCarBitmap && currentCarBitmap->GetCar() != cur) {
        currentCarBitmap.reset();
        carBitmap = nullptr;
    }
    if (!currentCarBitmap && cur) {
        currentCarBitmap = std::make_unique<CarBitmap>(cur, nullptr, cur->GetColor());
        carBitmap = currentCarBitmap.get();
        if (carBitmap) carBitmap->SetShadowVisible(false);
    }
    return currentCarBitmap.get();
}



void GasStation::OnSceneExit() {
    // Reset gas station specific state
    sceneState = GAS_STATION_IDLE;
    currentCarBitmap.reset();

    // Reset pump drawable visibility to initial state
    auto pumpNoHandle = dynamic_cast<Bitmap*>(GetDrawable("pump_no_handle"));
    if (pumpNoHandle) {
        pumpNoHandle->SetVisible(false);
    }

    // Call parent CarScene implementation (which handles common animation state)
    CarScene::OnSceneExit();
}

void GasStation::FadeOut() {
    // Call exit handler when fading out
    OnSceneExit();
    
    // Call parent implementation
    Scene::FadeOut();
}

void GasStation::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    }
}
