#include "core/log.h"
#include "scenes/gas_station.h"
#include "scenes/gas_station_pump.h"
#include "core/state.h"
#include "core/game_text.h"
#include "parsers/libp.h"
#include "gfx/animated_bitmap.h"
#include "scenes/scene_layout.h"
#include "embedded_resources.h"
#include "parsers/carcolor.h"
#include "core/utils.h"

#include <random>
#include "ui/hud.h"
#include <algorithm>
#include <memory>
double carAnimationTimeGasStation = 0;

namespace {
Asset GasStationBackground() {
    if (state.GetAssets().GetGame() == SrGame::SR2)
        return (Asset){LibAssetRegular, SrLibFileAmigaLibP, SR2_GAS_STATION_SCREEN, SrGame::SR2};
    return (Asset){LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_SCREEN};
}

bool IsSr2() { return state.GetAssets().GetGame() == SrGame::SR2; }

// Where asset 143 lies on the 320x190 background, found by matching the pump silhouettes to the art.
constexpr SDL_Rect kSr2PumpMaskRect = {0, 94, 240, 87};

// Rows from here down are the background again, which puts the tires behind the pump bases and the gravel. The
// original crops at 175 minus a per-scene value (5 here, 12 at the drive-in); 180 is what looks right.
constexpr int kSr2CarCropY = 180;

// The game blits the two halves of asset 151 onto the background at these x positions (table at 0x263ffc), y 128.
constexpr int kSr2HoseX[2] = {56, 176};
constexpr int kSr2HoseY = 128;
}

GasStation::GasStation()
    : CarScene(GasStationBackground(), StockPaletteAmigaGasStation) {
    zones = SceneLayout::Load(IsSr2() ? embedded_gas_station_sr2_toml : embedded_gas_station_toml);

    // Street Rod swaps the cursor while the handle is held; Street Rod 2 does not.
    auto setPointer = [this](SrLibAmigaSpriteAsset p) { if (!IsSr2()) SetPointer(p); };

    auto onPump = [this, setPointer] {
        if (!CanInteract()) return;
        if (carBitmap && sceneState == GAS_STATION_IDLE) {
            sceneState = GAS_STATION_HANDLEOFF;
            // Gas cap only shows once the handle is lifted off the pump.
            carBitmap->SetGasCapRemoved(true);

            if (pumpFilling) pumpFilling->SetVisible(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(true);
        }
        setPointer(SrLibAmigaSpriteAssetMousePointerPump);
    };
    if (IsSr2()) {
        // With a nozzle out, any hose strip puts it back; broke or already filled, it leaves the station instead.
        auto nozzleHeld = [this] {
            if (sceneState != GAS_STATION_HANDLEOFF) return false;
            if (state.GetBankroll() < 1 || hasPumpedAnyFuel) {
                TriggerCarExit();
                return true;
            }
            sceneState = GAS_STATION_IDLE;
            carBitmap->SetGasCapRemoved(false);
            SetNozzleOut(activePump, false);
            activePump = -1;
            return true;
        };
        auto sr2Pump = [this, onPump, nozzleHeld](int pump, int perTick) {
            if (!CanInteract() || !carBitmap || nozzleHeld()) return;
            if (sceneState == GAS_STATION_IDLE) {
                sr2PerTick = perTick;
                activePump = pump;
                SetNozzleOut(pump, true);
                onPump();
            }
        };
        for (const char* id : {"dead_left", "dead_right"}) {
            GetZone(id).onClick = [this, nozzleHeld] {
                if (!CanInteract() || !carBitmap || nozzleHeld()) return;
                ShowPumpMessage(GameText::Msg(GameText::USE_ANOTHER_PUMP));
            };
        }
        for (const char* id : {"premium_sign", "regular_sign"}) {
            GetZone(id).onClick = [this, nozzleHeld] {
                if (!CanInteract() || !carBitmap || nozzleHeld()) return;
                ShowPumpMessage(GameText::Msg(GameText::TAKE_A_PUMP));
            };
        }
        GetZone("pump").onClick = [sr2Pump] { sr2Pump(0, SR2_PREMIUM_PER_TICK); };
        GetZone("pump_regular").onClick = [sr2Pump] { sr2Pump(1, SR2_REGULAR_PER_TICK); };
        GetZone("back_to_garage").onClick = [this] {
            if (!CanInteract() || !carBitmap || animatingCar) return;
            if (sceneState != GAS_STATION_IDLE) return;
            // Event 8 only leaves with more than 4 units in the tank.
            Car* car = state.GetCurrentCar();
            if (!car) return;
            if (car->GetFuelTenths() <= 4) {
                ShowPumpMessage(GameText::Msg(GameText::GET_GAS_FIRST));
                return;
            }
            TriggerCarExit();
        };
    } else {
        GetZone("pump").onClick = onPump;
    }
    // ponytail: the original uses three click flags; with two zones this click inserts and starts, and a second
    // click mid-fill pulls the handle back early.
    GetZone("gas_cap").onClick = [this, setPointer] {
        if (!CanInteract() || !carBitmap) return;
        if (sceneState == GAS_STATION_HANDLEOFF) {
            // Refuses when the tank is full or the next tick is unaffordable.
            Car* car = state.GetCurrentCar();
            bool tankFull = car && TankFull(car);
            bool cantAfford = state.GetBankroll() < pricePerTick;
            if (tankFull || cantAfford) {
                ShowPumpMessage(GameText::Msg(cantAfford ? GameText::GAS_NOT_FREE : GameText::TANK_FULL));
            } else {
                sceneState = GAS_STATION_PUMPING;
                StartPumpDisplay();
                carBitmap->SetGasCapRemoved(true);
                if (pumpNoHandle) pumpNoHandle->SetVisible(true);
            }
        } else if (sceneState == GAS_STATION_PUMPING) {
            // For putting the handle back early with room and money left.
            Car* car = state.GetCurrentCar();
            if (car && !TankFull(car) &&
                state.GetBankroll() >= pricePerTick) {
                ShowPumpMessage(GameText::Msg(GameText::PUMP_NOT_FULL));
            }
            sceneState = GAS_STATION_IDLE;
            StopPumpDisplay();
            SetPumpSound(false);
            carBitmap->SetGasCapRemoved(false);
            SetNozzleOut(activePump, false);
            if (pumpFilling) pumpFilling->SetVisible(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(false);
        }
        setPointer(SrLibAmigaSpriteAssetMousePointerHand);
    };
    GetZone("rag").onClick = [this, setPointer] {
        if (!CanInteract()) return;
        setPointer(SrLibAmigaSpriteAssetMousePointerRag);
    };
    for (const char* id : {"car_window_rear", "car_window_front"}) {
        GetZone(id).onClick = [this, id, setPointer] {
            if (!CanInteract() || !ragHeld) return;
            setPointer(SrLibAmigaSpriteAssetMousePointerRag);
            StartTwinkle(id);
        };
    }
    // The trigger is unconfirmed; best guess is clicking Gus.
    if (!IsSr2() && ZoneById("attendant")) {
        GetZone("attendant").onClick = [this] {
            if (!CanInteract()) return;
            ShowPumpMessage(GameText::Msg(GameText::PUMP_GET_OUT));
        };
    }

    carx = -GAME_WIDTH;
    cary = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().height,
                                        state.GetAssets().GetGame());

    auto attendantLine = std::make_unique<Bubble>(
        GameText::Msg(GameText::PUMP_FULL_SERVICE), BUBBLE_LEFT, true);
    attendantBubble = attendantLine.get();
    bubbles.push_back(std::move(attendantLine));

    // Text set per-trigger by ShowPumpMessage. Messages 69-71 all use box 0,
    // the narrow bubble.
    auto pumpMsg = std::make_unique<Bubble>("", BUBBLE_RIGHT, true);
    pumpMsg->SetVisible(false);
    pumpMessageBubble = pumpMsg.get();
    bubbles.push_back(std::move(pumpMsg));
}

GasStation::~GasStation() {
}

void GasStation::Init(bool initialDebugMode) {
    Scene::Init(initialDebugMode);

    SetAudioManager(state.GetAssets().GetAudioManager());

    if (IsSr2()) {
        // The jockey is seven 32x32 frames; frame 0 is already painted into the background at this spot.
        Bitmap* sheet = state.GetAssets().GetBitmapAsset(
            {LibAssetSpritesheet, SrLibFileAmigaLibP, SR2_GAS_STATION_ATTENDANT_ANIMATION, SrGame::SR2});
        attendantAnim = std::make_unique<AnimatedBitmap>(sheet, SDL_Rect{160, 108, 32, 32});
        attendantAnim->SetAnimationFrames(32, 32, 7, 1);
        attendantAnim->SetSpeed(0.2f);   // seven frames, 10 ticks of 1/50 s each (FUN_0025bfb8)
        attendantAnim->SetLoop(false);
        attendantAnim->SetVisible(true);

        // The pump hoses and nozzle holders are not in the background art; the game patches them on (the strip's
        // black is empty space, so the surface is blended), and the mask below is cut from the result.
        Bitmap* hoses = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, SR2_GAS_STATION_PUMP_HOSES, SrGame::SR2});
        SDL_Surface* hoseSurface = hoses->GetSurface();
        SDL_Surface* bgSurface = background->GetSurface();
        auto copyStrip = [&](int pump) {
            SDL_Rect r = {kSr2HoseX[pump], kSr2HoseY, 16, 60};
            SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 16, 60, 32, bgSurface->format->format);
            SDL_SetSurfaceBlendMode(bgSurface, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(bgSurface, &r, s, nullptr);
            SDL_SetSurfaceBlendMode(bgSurface, SDL_BLENDMODE_BLEND);
            return s;
        };
        for (int pump = 0; pump < 2; ++pump) nozzleStrip[pump][1].reset(copyStrip(pump));
        for (int half = 0; half < 2; ++half) {
            SDL_Rect src = {half * 16, 0, 16, 60};
            SDL_Rect dst = {kSr2HoseX[half], kSr2HoseY, 16, 60};
            SDL_BlitSurface(hoseSurface, &src, bgSurface, &dst);
        }
        for (int pump = 0; pump < 2; ++pump) nozzleStrip[pump][0].reset(copyStrip(pump));
        background->UpdateFromSurface();

        sr2PumpMask = state.GetAssets().GetBitmapAsset(
            {LibAssetMask, SrLibFileAmigaLibP, SR2_GAS_STATION_PUMP_MASK, SrGame::SR2});
        RebuildSr2Overlays();

        hud->SetVisible(true);
        initStartTime = GameTicks();
        return;
    }

    auto maskBitmap = state.GetAssets().GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, GUS_GAS_MASK});
    maskBitmap->SetPosition(0, 107);
    const SDL_Rect maskRect = {0, 107, 312, 69};
    maskedBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 312, 69, 32, SDL_PIXELFORMAT_RGBA8888));
    background->CreateMask(maskBitmap->GetSurface(), maskedBackground.get(), maskRect);
    maskedBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedBackground.get()));
    SDL_SetTextureBlendMode(maskedBackgroundTexture.get(), SDL_BLENDMODE_BLEND);

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

    // 338 is the background slice with the handle painted out, drawn behind the car while the handle is off.
    pumpNoHandle = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, GUS_GAS_PUMP2});
    pumpNoHandle->SetPosition(PUMP_POS.x, PUMP_POS.y);
    pumpNoHandle->SetVisible(false);

    // Mask 341 covers the hanging handle too, so with the handle off 339 replaces its output in the pump rect.
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
    // Only the stand-up animation waits for the idle countdown.
    attendantAnim->SetVisible(true);

    hud->SetVisible(true);

    initStartTime = GameTicks();
}


SDL_Point GasStation::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GAS_STATION);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GAS_STATION, state.GetCurrentCar()->GetModel().height,
                                        state.GetAssets().GetGame());
    return {x, y};
}

void GasStation::DrawScene() {
    currentTime = GameTicks() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);
    // UpdateCarAnimation can trigger OnCarExitComplete -> MoveToScene
    // synchronously, switching the active scene away mid-call.
    if (state.GetCurrentScene() != this) return;

    if (attendantAnim) {
        attendantAnim->Update(deltaTime);
    }

    Scene::DrawScene();

    if (initialDelay) {
        if (GameTicks() - fadeCompleteTime >= 2000) {
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

    DrawTwinkle(deltaTime);

    for (auto& b : bubbles) {
        b->Draw();
    }

    DrawDebugZones();
    hud->Draw();
}

void GasStation::DrawMaskAfterCar() {
    // Street Rod 2 has only the pump mask; the handle, road edge and pump lights below are Street Rod's.
    if (IsSr2()) {
        SDL_Rect rect = kSr2PumpMaskRect;
        SDL_RenderCopy(renderer, roadMaskTexture.get(), NULL, &roadMaskSrcRect);
        SDL_RenderCopy(renderer, maskedBackgroundTexture.get(), NULL, &rect);
        CarScene::DrawMaskAfterCar();
        if (pumpFilling && pumpFilling->IsVisible()) pumpFilling->Draw();
        if (sceneState == GAS_STATION_PUMPING && activePump >= 0) {
            const int dx = activePump * 162;
            for (auto [x, lit] : {std::pair{51, pumpDisplayFastLit}, std::pair{48, pumpDisplaySlowLit}}) {
                const Uint8 v = lit ? 153 : 0;
                SDL_Rect dot = {x + dx, 142, 2, 1};
                SDL_SetRenderDrawColor(renderer, v, v, v, 255);
                SDL_RenderFillRect(renderer, &dot);
            }
        }
        return;
    }
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

    if (pumpFilling && pumpFilling->IsVisible()) {
        pumpFilling->Draw();
    }

    // Lit is PAL_GAS index 0xf (0x0999): 0x9 * 17 = 153.
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
    if (IsSr2()) {
        const bool wide = state.GetAssets().GetBetterClickboxesRef();
        SetZoneRect("pump", wide ? SDL_Rect{21, 124, 44, 62} : SDL_Rect{52, 124, 12, 34});
        SetZoneRect("pump_regular", wide ? SDL_Rect{177, 124, 45, 62} : SDL_Rect{177, 124, 9, 34});
        SetZoneText("pump", wide ? "High octane gasoline" : "The pump");
        SetZoneText("pump_regular", wide ? "Regular gasoline" : "The pump");
    }
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
            audioManager->SetMufflerRemoved(car->IsMufflerRemoved());
            engineBase = 600 + std::uniform_int_distribution<int>(0, 9)(RandomEngine());
            lastAnimationEnginePeriod = -1;
            ApplyAnimationEngine(false, false);
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
        }
        TriggerCarExit();
        return;
    }

    switch (sceneState) {
        case GAS_STATION_IDLE:
        case GAS_STATION_HANDLEOFF: {
            if (hasPumpedAnyFuel) return;  // already got gas this visit; auto-exit takes it from here
            idleTimer += deltaTime;
            if (idleTimer < (IsSr2() ? SR2_IDLE_TIMEOUT_SEC : IDLE_TIMEOUT_SEC)) return;
            idleTimer = 0;
            if (!IsSr2()) pricePerTick *= 2;   // the SR2 pump charges a flat dollar a tick
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

            pumpDisplayFastLit = (pumpDisplayCounter & 1) != 0;
            pumpDisplayCounter++;
            if ((pumpDisplayCounter & 3) != 0) return;
            pumpDisplaySlowLit = (pumpDisplayCounter & 4) != 0;

            GasPumpTick tick = IsSr2()
                ? ApplySr2GasPumpTick(car->GetFuelTenths(), state.GetBankroll(), sr2PerTick, SR2_TANK_CAP)
                : ApplyGasPumpTick(car->GetFuelTenths(), state.GetBankroll(),
                                   pricePerTick, FUEL_PER_TICK_TENTHS, TANK_CAP_TENTHS);
            if (tick.dispensed) {
                car->SetFuelTenths(tick.fuelTenths);
                state.SetBankroll(tick.bankroll);
                hasPumpedAnyFuel = true;
            }

            if (tick.done) {
                sceneState = GAS_STATION_IDLE;
                StopPumpDisplay();
                SetPumpSound(false);
                carBitmap->SetGasCapRemoved(false);
                SetNozzleOut(activePump, false);
                if (pumpFilling) pumpFilling->SetVisible(false);
                if (pumpNoHandle) pumpNoHandle->SetVisible(false);  // handle back on the pump
                if (attendantBubble) attendantBubble->SetVisible(false);
                leaveTimer = 0.0;
            }
            break;
        }
    }
}

// Cuts the pump mask and the crop strip from the current background.
void GasStation::RebuildSr2Overlays() {
    const SDL_Rect rect = kSr2PumpMaskRect;
    maskedBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, rect.w, rect.h, 32, SDL_PIXELFORMAT_RGBA8888));
    background->CreateMask(sr2PumpMask->GetSurface(), maskedBackground.get(), rect);
    maskedBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedBackground.get()));
    SDL_SetTextureBlendMode(maskedBackgroundTexture.get(), SDL_BLENDMODE_BLEND);

    // Below the crop line the background is drawn over the car.
    roadMaskSrcRect = {0, kSr2CarCropY, GAME_WIDTH, 190 - kSr2CarCropY};
    roadMaskDestRect = {0, 0, GAME_WIDTH, 190 - kSr2CarCropY};
    roadMaskSurface.reset(SDL_CreateRGBSurface(0, roadMaskSrcRect.w, roadMaskSrcRect.h, 32, 0, 0, 0, 0));
    SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
    roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
    SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
}

void GasStation::SetNozzleOut(int pump, bool out) {
    if (pump < 0 || pump > 1 || nozzleOut[pump] == out || !nozzleStrip[pump][0]) return;
    nozzleOut[pump] = out;
    SDL_Surface* strip = nozzleStrip[pump][out ? 1 : 0].get();
    SDL_Surface* bgSurface = background->GetSurface();
    SDL_Rect dst = {kSr2HoseX[pump], kSr2HoseY, 16, 60};
    SDL_SetSurfaceBlendMode(strip, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(strip, nullptr, bgSurface, &dst);
    SDL_SetSurfaceBlendMode(strip, SDL_BLENDMODE_BLEND);
    background->UpdateFromSurface();
    RebuildSr2Overlays();
}

bool GasStation::TankFull(const Car* car) const {
    if (IsSr2()) return SR2_TANK_CAP - car->GetFuelTenths() < sr2PerTick;
    return car->GetFuelTenths() >= TANK_CAP_TENTHS;
}

void GasStation::StartTwinkle(const char* windowZone) {
    if (!state.GetAssets().GetExtraAnimationsRef()) return;
    const SDL_Rect r = GetZone(windowZone).GetRect();
    twinklePos = {r.x + r.w / 2, r.y + r.h / 2};
    twinkleTime = 0.0;
    if (audioManager) audioManager->PlayChime(0, 0x96, 3);
}

void GasStation::DrawTwinkle(double deltaTime) {
    if (twinkleTime < 0) return;
    twinkleTime += deltaTime;
    const int step = static_cast<int>(twinkleTime / TWINKLE_STEP_SEC);
    if (step >= 2 * TWINKLE_FRAMES) {
        twinkleTime = -1.0;
        return;
    }
    const int f = step < TWINKLE_FRAMES ? step : 2 * TWINKLE_FRAMES - 1 - step;
    const int len = f + 2;
    for (int d = -len; d <= len; d++) {
        const Uint8 v = 255 - 80 * std::abs(d) / len;
        SDL_SetRenderDrawColor(renderer, v, v, v, 255);
        SDL_RenderDrawPoint(renderer, twinklePos.x + d, twinklePos.y);
        if (d != 0) SDL_RenderDrawPoint(renderer, twinklePos.x, twinklePos.y + d);
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
    // Only before the player has started with the pump, so e.g. wiping a window cannot cancel a fill.
    if (event->type == SDL_MOUSEBUTTONDOWN && sceneState == GAS_STATION_IDLE && CanInteract()) {
        ragHeld = GetPointerType() == SrLibAmigaSpriteAssetMousePointerRag;
        if (carBitmap) {
            carBitmap->SetGasCapRemoved(false);
            if (pumpNoHandle) pumpNoHandle->SetVisible(false);
        }
        if (!IsSr2()) SetPointer(SrLibAmigaSpriteAssetMousePointerHand);
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
            stepAccumulator = 0.0;
            cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), car->GetModel().height,
                                        state.GetAssets().GetGame());
            animatingCar = true;
            currentAnimationType = animType;
            // Animation mode 1, as at the garage (0x246332): the engine runs while the car pulls in.
            if (audioManager) {
                audioManager->SetMufflerRemoved(car->IsMufflerRemoved());
                engineBase = 600 + std::uniform_int_distribution<int>(0, 9)(RandomEngine());
                lastAnimationEnginePeriod = -1;
                ApplyAnimationEngine(true, false);
                audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
            }
        }
    }
}

void GasStation::OnCarAnimationComplete() {
    CarScene::OnCarAnimationComplete();
}

void GasStation::OnCarExitComplete() {
    if (IsSr2()) {
        if (Car* car = state.GetCurrentCar()) car->SetFuelTenths(Sr2FuelOnLeaving(car->GetFuelTenths()));
    }
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

        // Palette-matched to this car; refreshed per visit since Init() only runs once.
        const SrGame game = IsSr2() ? SrGame::SR2 : SrGame::SR1;
        byte_vector& libFile = state.GetAssets().GetLibPData(game);
        std::vector<uint16_t> carPalette = GetStandardCarColor(cur->GetColor());
        pumpFilling = state.GetAssets().GetBitmapAssetWithPalette(
            libFile, {LibAssetRegular, SrLibFileAmigaLibP, IsSr2() ? SR2_GUS_GAS_PUMP_FILLING : GUS_GAS_PUMP_FILLING,
                      game}, carPalette);
        pumpFilling->SetDimensions(24, 12);
        pumpFilling->SetVisible(false);
    }
    return currentCarBitmap.get();
}



void GasStation::OnSceneExit() {
    sceneState = GAS_STATION_IDLE;
    SetNozzleOut(activePump, false);
    activePump = -1;
    currentCarBitmap.reset();
    idleTimer = 0.0;
    leaveTimer = -1.0;
    pricePerTick = 1;
    hasPumpedAnyFuel = false;
    SetPumpSound(false);
    StartPumpDisplay();
    waitingForAttendant = false;

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

    CarScene::OnSceneExit();
}

void GasStation::FadeOut() {
    OnSceneExit();
    Scene::FadeOut();
}

void GasStation::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = GameTicks();
    }
}
