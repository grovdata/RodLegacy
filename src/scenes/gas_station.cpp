#include "core/log.h"
#include "scenes/gas_station.h"
#include "scenes/gas_station_pump.h"
#include "core/state.h"
#include "parsers/libp.h"
#include "gfx/animated_bitmap.h"
#include "scenes/scene_layout.h"
#include "embedded_resources.h"
#include "parsers/carcolor.h"
#include "ui/hud.h"
#include <algorithm>
#include <memory>
double carAnimationTimeGasStation = 0;

GasStation::GasStation()
    : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_SCREEN},
               StockPaletteAmigaGasStation) {
    zones = SceneLayout::Load(embedded_gas_station_toml);

    GetZone("pump").onClick = [this] {
        if (!CanInteract()) return;
        if (carBitmap && sceneState == GAS_STATION_IDLE) {
            sceneState = GAS_STATION_HANDLEOFF;
            // Gas cap only shows once the handle is lifted off the pump.
            carBitmap->SetGasCapRemoved(true);

            if (pumpFilling) pumpFilling->SetVisible(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(true);
        }
        SetPointer(SrLibAmigaSpriteAssetMousePointerPump);
    };
    // ponytail: the original walks handle-grab -> handle-at-cap through three
    // separate click flags; RodLegacy has two zones (pump, gas_cap), so this
    // is the "insert and start" click. Clicking it again mid-fill pulls the
    // handle back out early, matching the manual's two-click sequence.
    GetZone("gas_cap").onClick = [this] {
        if (!CanInteract() || !carBitmap) return;
        if (sceneState == GAS_STATION_HANDLEOFF) {
            // Message 70 ("Pay attention, dummy!"): confirmed from
            // GasStationPumpLoop, refuses to start dispensing when the tank
            // is already full or the player can't cover the next tick -
            // see gas_station.md. No state change on refusal.
            Car* car = state.GetCurrentCar();
            bool tankFull = car && car->GetFuelTenths() >= TANK_CAP_TENTHS;
            bool cantAfford = state.GetBankroll() < pricePerTick;
            if (tankFull || cantAfford) {
                ShowPumpMessage("  Pay attention, dummy`!");
            } else {
                sceneState = GAS_STATION_PUMPING;
                StartPumpDisplay();
                carBitmap->SetGasCapRemoved(true);
                if (pumpNoHandle) pumpNoHandle->SetVisible(true);
            }
        } else if (sceneState == GAS_STATION_PUMPING) {
            // Message 69 ("It's not full, speedy!"): confirmed from
            // GasStationPumpLoop - GasPumpDisplayBlink(STOP) fires right
            // before this check, so it belongs to putting the handle back
            // EARLY, not grabbing it. Shown only if the tank could still
            // have taken more and there was money left to pay for it -
            // see gas_station.md.
            Car* car = state.GetCurrentCar();
            if (car && car->GetFuelTenths() < TANK_CAP_TENTHS &&
                state.GetBankroll() >= pricePerTick) {
                ShowPumpMessage("It's not full, speedy`!");
            }
            // Handle back on the pump: cap closes, asset 307 overlay hides.
            sceneState = GAS_STATION_IDLE;
            StopPumpDisplay();
            SetPumpSound(false);
            carBitmap->SetGasCapRemoved(false);
            if (pumpFilling) pumpFilling->SetVisible(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(false);
        }
        SetPointer(SrLibAmigaSpriteAssetMousePointerHand);
    };
    GetZone("rag").onClick = [this] {
        if (!CanInteract()) return;
        SetPointer(SrLibAmigaSpriteAssetMousePointerRag);
    };
    // Message 71 ("Get out of my face, dummy!"): confirmed message ID from
    // GasStationPumpLoop, but its exact trigger wasn't pinned down in the
    // disassembly (reached via the scene's input switch, not a branch this
    // session could trace back to a specific zone) - best guess is clicking
    // Gus himself, which had no handler at all before this. See gas_station.md.
    GetZone("attendant").onClick = [this] {
        if (!CanInteract()) return;
        ShowPumpMessage("Get out of my face, dummy`!");
    };

    carx = -GAME_WIDTH;
    cary = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().height);

    // Arrow points right, toward the attendant sitting at x=200.
    auto attendantLine = std::make_unique<Bubble>(
        "Yo, you need full service, dummy`!", BUBBLE_RIGHT, 90, 30, true);
    attendantBubble = attendantLine.get();
    bubbles.push_back(std::move(attendantLine));

    // Text set per-trigger by ShowPumpMessage; a separate y so it never
    // stacks visually with attendantBubble above.
    auto pumpMsg = std::make_unique<Bubble>("", BUBBLE_RIGHT, 90, 45, true);
    pumpMsg->SetVisible(false);
    pumpMessageBubble = pumpMsg.get();
    bubbles.push_back(std::move(pumpMsg));
}

GasStation::~GasStation() {
}

void GasStation::Init(bool initialDebugMode) {
    Scene::Init(initialDebugMode);

    // Set audio manager for engine sound
    SetAudioManager(state.GetAssets().GetAudioManager());

    // Create a mask for gas pump
    auto maskBitmap = state.GetAssets().GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, GUS_GAS_MASK});
    maskBitmap->SetPosition(0, 107);
    const SDL_Rect maskRect = {0, 107, 312, 69};
    maskedBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 312, 69, 32, SDL_PIXELFORMAT_RGBA8888));
    background->CreateMask(maskBitmap->GetSurface(), maskedBackground.get(), maskRect);
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

    // Asset-owned, non-owning ref. 338 is the background slice with the pump's
    // hanging handle painted out; it draws behind the car while the handle is
    // off, since the background bitmap itself still has the handle in place.
    pumpNoHandle = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_PUMP2});
    pumpNoHandle->SetPosition(PUMP_POS.x, PUMP_POS.y);
    pumpNoHandle->SetVisible(false);

    // Handle-off foreground. Mask 341's pump region covers the hanging handle
    // too, so with the handle off it painted what is behind the handle (the
    // brick wall) over the car's nose. Asset 339 is the pump's front edge
    // without the handle, so inside the pump rect it replaces the mask's
    // output outright - art and alpha both.
    pumpSideMask = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_PUMP3_MASK});
    maskedBackgroundNoHandle.reset(SDL_CreateRGBSurfaceWithFormat(0, 312, 69, 32, SDL_PIXELFORMAT_RGBA8888));
    SDL_SetSurfaceBlendMode(maskedBackground.get(), SDL_BLENDMODE_NONE);
    SDL_BlitSurface(maskedBackground.get(), nullptr, maskedBackgroundNoHandle.get(), nullptr);
    SDL_SetSurfaceBlendMode(maskedBackground.get(), SDL_BLENDMODE_BLEND);
    SDL_Surface* pumpFront = pumpSideMask->GetSurface();
    SDL_Rect frontDst = {PUMP_POS.x, PUMP_POS.y - maskRect.y, PUMP_SIZE.x, PUMP_SIZE.y};
    SDL_SetSurfaceBlendMode(pumpFront, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(pumpFront, nullptr, maskedBackgroundNoHandle.get(), &frontDst);
    SDL_SetSurfaceBlendMode(pumpFront, SDL_BLENDMODE_BLEND);
    maskedBackgroundNoHandleTexture.reset(
        SDL_CreateTextureFromSurface(renderer, maskedBackgroundNoHandle.get()));
    SDL_SetTextureBlendMode(maskedBackgroundNoHandleTexture.get(), SDL_BLENDMODE_BLEND);

    Bitmap* attendantSheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, GUS_GAS_ATTENDANT});
    SDL_Rect attendantRect = {200, 90, 47, 47};
    attendantAnim = std::make_unique<AnimatedBitmap>(attendantSheet, attendantRect);
    attendantAnim->SetAnimationFrames(47, 47, 4, 1); // 2x2 grid
    attendantAnim->SetSpeed(0.5f); // 0.5s per frame
    attendantAnim->SetLoop(false);
    // Gus sits in the scene the whole time on frame 0; only the stand-up
    // animation waits for the idle countdown.
    attendantAnim->SetVisible(true);

    // The original shows the bottom status bar here too (GasStationPumpLoop
    // calls SetupStatusBar(3) via 0x002259fa), with the bankroll string
    // redrawn on every charge.
    hud->SetVisible(true);

    initStartTime = SDL_GetTicks64();
}


SDL_Point GasStation::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GAS_STATION);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().height);
    return {x, y};
}

void GasStation::DrawScene() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);
    // UpdateCarAnimation can trigger OnCarExitComplete -> MoveToScene
    // synchronously, switching the active scene away mid-call.
    if (state.GetCurrentScene() != this) return;

    // Update attendant animation
    if (attendantAnim) {
        attendantAnim->Update(deltaTime);
    }

    Scene::DrawScene();

    // Wait 2s
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
        }
        return;
    }

    UpdatePumping(deltaTime);

    DrawAll();

    if (attendantAnim && attendantAnim->IsVisible()) {
        attendantAnim->Draw();
    }

    if (pumpNoHandle && pumpNoHandle->IsVisible()) {
        pumpNoHandle->Draw();
    }

    if (carBitmap != nullptr) {
        carBitmap->Draw();
    }

    for (auto& b : bubbles) {
        b->Draw();
    }

    DrawDebugZones();
    hud->Draw();
}

void GasStation::DrawMaskAfterCar() {
    // Draw the gas pump mask
    const bool handleOff = pumpNoHandle && pumpNoHandle->IsVisible();
    SDL_Rect rect = {0, 107, 312, 69};
    SDL_RenderCopy(renderer,
                   (handleOff && maskedBackgroundNoHandleTexture)
                       ? maskedBackgroundNoHandleTexture.get()
                       : maskedBackgroundTexture.get(),
                   NULL, &rect);

    if (roadMaskSurface && roadMaskTexture) {
        SDL_BlitSurface(surface.get(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
        roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
        SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer, roadMaskTexture.get(), NULL, &roadMaskSrcRect);
    }

    CarScene::DrawMaskAfterCar();

    // Asset 306 (the pump front with the handle) used to be drawn here; it is
    // pixel-identical to the background it covers, so the masked overlay above
    // already puts it in front of the car.

    if (pumpFilling && pumpFilling->IsVisible()) {
        pumpFilling->Draw();
    }

    // Pump display: a direct pixel fill in the original (see
    // PUMP_DISPLAY_FAST's comment). Lit is PAL_GAS index 0xf (0x0999)
    // converted to RGB: 0x9 * 17 = 153; unlit is index 0, black.
    if (sceneState == GAS_STATION_PUMPING && !waitingForAttendant) {
        auto drawDot = [](const SDL_Rect& rect, bool lit) {
            const Uint8 v = lit ? 153 : 0;
            SDL_SetRenderDrawColor(renderer, v, v, v, 255);
            SDL_RenderFillRect(renderer, &rect);
        };
        drawDot(PUMP_DISPLAY_FAST, pumpDisplayFastLit);
        drawDot(PUMP_DISPLAY_SLOW, pumpDisplaySlowLit);
    }
}

void GasStation::UpdateCarAnimation(double deltaTime) {
    CarScene::UpdateCarAnimation(deltaTime);
    if (carBitmap == nullptr || state.GetCurrentCar() == nullptr) {
        return;
    }

    CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
    CarClickBoxes_t clickBoxes = metadata.clickBoxes;
    
    SDL_Rect gasCapRect = {carx + clickBoxes.clickBoxGasCap.x, cary + clickBoxes.clickBoxGasCap.y, clickBoxes.clickBoxGasCap.width, clickBoxes.clickBoxGasCap.height};
    SetZoneRect("gas_cap", gasCapRect);

    // Right-justified over the gas cap: same right edge, same top.
    if (pumpFilling) {
        pumpFilling->SetPosition(gasCapRect.x + gasCapRect.w - 28, gasCapRect.y - 5);
    }

    SDL_Rect rearWindowRect = {metadata.offsets.offsetXRearTire + clickBoxes.clickBoxRearWindow.x, cary + clickBoxes.clickBoxRearWindow.y, clickBoxes.clickBoxRearWindow.width, clickBoxes.clickBoxRearWindow.height};
    SetZoneRect("car_window_rear", rearWindowRect);

    SDL_Rect frontWindowRect = {metadata.offsets.offsetXRearTire + clickBoxes.clickBoxFrontWindow.x, cary + clickBoxes.clickBoxFrontWindow.y, clickBoxes.clickBoxFrontWindow.width, clickBoxes.clickBoxFrontWindow.height};
    SetZoneRect("car_window_front", frontWindowRect);
}

void GasStation::UpdatePumping(double deltaTime) {
    if (!carBitmap || animatingCar) return;
    Car* car = state.GetCurrentCar();
    if (!car) return;

    if (leaveTimer >= 0) {
        leaveTimer += deltaTime;
        if (leaveTimer < LEAVE_DELAY_SEC) return;
        leaveTimer = -1.0;
        if (audioManager) {
            audioManager->UpdateEnginePeriod(605);
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
        }
        TriggerCarExit();
        return;
    }

    switch (sceneState) {
        case GAS_STATION_IDLE:
        case GAS_STATION_HANDLEOFF: {
            if (hasPumpedAnyFuel) return;  // already got gas this visit; auto-exit takes it from here
            idleTimer += deltaTime;
            if (idleTimer < IDLE_TIMEOUT_SEC) return;
            idleTimer = 0;
            pricePerTick *= 2;
            sceneState = GAS_STATION_PUMPING;
            StartPumpDisplay();
            carBitmap->SetGasCapRemoved(true);
            if (pumpNoHandle) pumpNoHandle->SetVisible(true);
            if (attendantAnim) {
                attendantAnim->SetFrame(0);
                attendantAnim->Play();
            }
            waitingForAttendant = true;
            break;
        }
        case GAS_STATION_PUMPING: {
            if (attendantAnim && attendantAnim->IsPlaying()) return;

            if (waitingForAttendant) {
                waitingForAttendant = false;
                if (attendantBubble) attendantBubble->SetVisible(true);
            }

            if (pumpFilling) pumpFilling->SetVisible(true);
            SetPumpSound(true);

            loopTimer += deltaTime;
            if (loopTimer < LOOP_ITERATION_SEC) return;
            loopTimer -= LOOP_ITERATION_SEC;

            // Dispense fuel every 4th blink from the dispplay
            pumpDisplayFastLit = (pumpDisplayCounter & 1) != 0;
            pumpDisplayCounter++;
            if ((pumpDisplayCounter & 3) != 0) return;
            pumpDisplaySlowLit = (pumpDisplayCounter & 4) != 0;

            GasPumpTick tick = ApplyGasPumpTick(car->GetFuelTenths(), state.GetBankroll(),
                                                 pricePerTick, FUEL_PER_TICK_TENTHS, TANK_CAP_TENTHS);
            if (tick.dispensed) {
                car->SetFuelTenths(tick.fuelTenths);
                state.SetBankroll(tick.bankroll);
                hud->Refresh();
                hasPumpedAnyFuel = true;
            }

            if (tick.done) {
                // Tank full or out of money
                sceneState = GAS_STATION_IDLE;
                StopPumpDisplay();
                SetPumpSound(false);
                carBitmap->SetGasCapRemoved(false);
                if (pumpFilling) pumpFilling->SetVisible(false);
                if (pumpNoHandle) pumpNoHandle->SetVisible(false);  // handle back on the pump
                if (attendantBubble) attendantBubble->SetVisible(false);
                leaveTimer = 0.0;
            }
            break;
        }
    }
}

void GasStation::SetPumpSound(bool on) {
    if (on == pumpSoundOn || !audioManager) return;
    pumpSoundOn = on;
    if (on) {
        audioManager->StartGasPumpLoop();
    } else {
        audioManager->StopGasPumpLoop();
    }
}

void GasStation::StartPumpDisplay() {
    loopTimer = 0;
    pumpDisplayCounter = 0;
    pumpDisplayFastLit = false;
    pumpDisplaySlowLit = false;
}

void GasStation::StopPumpDisplay() {
    pumpDisplayFastLit = false;
    pumpDisplaySlowLit = false;
}

void GasStation::ShowPumpMessage(const std::string& text) {
    if (!pumpMessageBubble) return;
    pumpMessageBubble->SetText(text);
    pumpMessageBubble->SetVisible(true);
}

void GasStation::UpdateDebugInfo() {
    if (!debugMode) return;

    CarScene::UpdateDebugInfo();

    AddDebugValue("gas station", "");
    AddDebugValue("car pos", std::to_string(carx) + ", " + std::to_string(cary));

    if (carBitmap == nullptr) {
        AddDebugValue("car", "not on screen");
        return;
    }

    const CarMetadata_t metadata =
        state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
    const CarClickBoxes_t clickBoxes = metadata.clickBoxes;

    auto boxText = [](const auto& box) {
        return std::to_string(box.x) + ", " + std::to_string(box.y) + "  " +
               std::to_string(box.width) + "x" + std::to_string(box.height);
    };

    AddDebugValue("gas cap", std::to_string(metadata.offsets.offsetXGasCap) + ", " +
                             std::to_string(metadata.offsets.offsetYGasCap));
    AddDebugValue("click boxes", "");
    AddDebugValue("rear window", boxText(clickBoxes.clickBoxRearWindow));
    AddDebugValue("front window", boxText(clickBoxes.clickBoxFrontWindow));
}

bool GasStation::PushEvent(SDL_Event* event) {
    // Only reset the handle/cap visuals on a stray click before the player
    // has started interacting with the pump - once handle-off or pumping,
    // clicks are routed through the pump/gas_cap zone handlers instead, so
    // e.g. wiping a window mid-fill does not silently cancel the fill.
    if (event->type == SDL_MOUSEBUTTONDOWN && sceneState == GAS_STATION_IDLE && CanInteract()) {
        if (carBitmap) {
            carBitmap->SetGasCapRemoved(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(false);
        }
        SetPointer(SrLibAmigaSpriteAssetMousePointerHand);
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

void GasStation::StartAnimateCar(CarPtr car, Driver*, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;

    if (car != nullptr && carBitmap == nullptr) {
        carBitmap = GetOrCreateCurrentCarBitmap();
        if (carBitmap) {
            carx = -GAME_WIDTH;
            carxFloat = carx;  // else UpdateCarAnimation resumes from wherever the previous visit's exit left it
            cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().height);
            animatingCar = true;
            currentAnimationType = animType;
        }
    }
}

void GasStation::OnCarAnimationComplete() {
    CarScene::OnCarAnimationComplete();
}

void GasStation::OnCarExitComplete() {
    sceneTransitioningTo = SCENE_GARAGE;
    FadeOut();
    state.MoveToScene(SCENE_GARAGE);
}


CarBitmap* GasStation::GetOrCreateCurrentCarBitmap() {
    const CarPtr& cur = state.GetCurrentCarPtr();
    if (currentCarBitmap && currentCarBitmap->GetCar() != cur.get()) {
        currentCarBitmap.reset();
        carBitmap = nullptr;
    }
    if (!currentCarBitmap && cur) {
        currentCarBitmap = std::make_unique<CarBitmap>(cur, nullptr, cur->GetColor());
        carBitmap = currentCarBitmap.get();
        if (carBitmap) carBitmap->SetShadowVisible(false);

        // Palette-matched to this car, same as gasCap/CAR_GAS_CAP in
        // carbitmap.cpp - refreshed here (car changed or OnSceneExit tore
        // the old one down) rather than in Init(), which the scene lifecycle
        // only ever calls once at program start, not per visit.
        byte_vector& libFile = state.GetAssets().GetLibPData();
        std::vector<uint16_t> carPalette = GetStandardCarColor(cur->GetColor());
        pumpFilling = state.GetAssets().GetBitmapAssetWithPalette(
            libFile, {LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_PUMP_FILLING}, carPalette);
        pumpFilling->SetDimensions(24, 12);
        pumpFilling->SetVisible(false);
    }
    return currentCarBitmap.get();
}



void GasStation::OnSceneExit() {
    // Reset gas station specific state
    sceneState = GAS_STATION_IDLE;
    currentCarBitmap.reset();
    idleTimer = 0.0;
    leaveTimer = -1.0;
    pricePerTick = 1;
    hasPumpedAnyFuel = false;
    SetPumpSound(false);
    StartPumpDisplay();
    waitingForAttendant = false;

    // Reset pump overlay visibility to initial state
    if (pumpNoHandle) {
        pumpNoHandle->SetVisible(false);
    }
    if (pumpFilling) {
        pumpFilling->SetVisible(false);
    }
    if (attendantAnim) {
        attendantAnim->Stop();
        attendantAnim->SetFrame(0);  // back to sitting; he stays visible
    }
    if (attendantBubble) {
        attendantBubble->SetVisible(false);
    }
    if (pumpMessageBubble) {
        pumpMessageBubble->SetVisible(false);
    }

    // Call parent CarScene implementation (which handles common animation state)
    CarScene::OnSceneExit();
}

void GasStation::FadeOut() {
    OnSceneExit();
    Scene::FadeOut();
}

void GasStation::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    }
    hud->Refresh();  // bankroll may have changed since the last visit
}
