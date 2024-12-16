#include "gas_station.h"
#include "state.h"
#include "parsers/libp.h"
double carAnimationTimeGasStation = 0;

GasStation::GasStation() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, GUS_GAS_SCREEN}) {
    isPumping = false;
    SDL_Rect pumpRect = {40, 105, 42, 73};
    Zone pump(ZONE_GAS_STATION_PUMP, strdup("The pump"), &pumpRect);
    
    SDL_Rect ragRect = {95, 176, 22, 14};
    Zone rag(ZONE_GAS_STATION_RAG, strdup("A rag"), &ragRect);

    SDL_Rect gasCapRect = {30, 30, 12, 12};
    Zone gasCap(ZONE_GAS_STATION_GAS_CAP, strdup("A gas cap"), &gasCapRect);

    SDL_Rect rearWindowRect = {80, 80, 24, 12};
    Zone rearWindow(ZONE_GAS_STATION_CAR_WINDOW_REAR, strdup("A rear window"), &rearWindowRect);

    SDL_Rect frontWindowRect = {120, 120, 24, 12};
    Zone frontWindow(ZONE_GAS_STATION_CAR_WINDOW_FRONT, strdup("A front window"), &frontWindowRect);

    zones.push_back(pump);
    zones.push_back(rag);
    zones.push_back(gasCap);
    zones.push_back(rearWindow);
    zones.push_back(frontWindow);

    int carYOffset = state.carData.GetCarRoofYOffset(state.GetCurrentCar()->GetModel());
    this->cary = carYOffset;
    this->cary = 105;

    //hud = new HUD(StockPalette::GUS_GAS_STATION, 20, 18);
    // Hud color text index 18
}

void GasStation::Init(bool initialDebugMode) {
    Scene::Init(initialDebugMode);

    // Create a mask for gas pump
    maskBitmap = state.assets.GetBitmapAsset({LibAssetMask, AssetLocationAmigaLibP, GUS_GAS_MASK});
    maskBitmap->SetPosition(0, 107);
    maskedBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 312, 69, 32, SDL_PIXELFORMAT_RGBA8888));
    background->CreateMask(maskBitmap->GetSurface(), maskedBackground.get(), {0, 107, 312, 69});
    maskedBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedBackground.get()));
    SDL_SetTextureBlendMode(maskedBackgroundTexture.get(), SDL_BLENDMODE_BLEND);



    attendant = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, GUS_GAS_ATTENDANT});
    attendant->SetDimensions(47, 47);
    attendant->SelectSprite(0, 0);
    attendant->SetPosition(200, 90);
    attendant->SetVisible(true);
    
    initStartTime = SDL_GetTicks64();
}

void GasStation::Action(ZoneId zone) {
    if (carBitmap){
        carBitmap->SetGasCapRemoved(false);
        carBitmap->SetPumpingHandleVisible(false);
    }
    
    if (zone == ZONE_GAS_STATION_PUMP) {
        if (carBitmap) {
            carBitmap->SetGasCapRemoved(true);
            carBitmap->SetPumpingHandleVisible(true);
        }
        currentPointer = PointerFuelPump;
    } else if (zone == ZONE_GAS_STATION_RAG) {
        currentPointer = PointerRag;
    } else {
        currentPointer = PointerHand;
    }
}

SDL_Point GasStation::GetCarIdlePosition() {
    return {35, 82};
}

SDL_Point GasStation::GetCarOffscreenPosition() {
    return {-320, 82};
}

void GasStation::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    Scene::Draw();

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
        return;
    }


    attendant->Draw();
    
    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();

        CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;
        
        SDL_Rect gasCapRect = {carx + clickBoxes.clickBoxGasCap.x, cary + clickBoxes.clickBoxGasCap.y, clickBoxes.clickBoxGasCap.width, clickBoxes.clickBoxGasCap.height};
        SetZoneRect(ZONE_GAS_STATION_GAS_CAP, &gasCapRect);

        SDL_Rect rearWindowRect = {carx + clickBoxes.clickBoxRearWindow.x, cary + clickBoxes.clickBoxRearWindow.y, clickBoxes.clickBoxRearWindow.width, clickBoxes.clickBoxRearWindow.height};
        SetZoneRect(ZONE_GAS_STATION_CAR_WINDOW_REAR, &rearWindowRect);

        SDL_Rect frontWindowRect = {carx + clickBoxes.clickBoxFrontWindow.x, cary + clickBoxes.clickBoxFrontWindow.y, clickBoxes.clickBoxFrontWindow.width, clickBoxes.clickBoxFrontWindow.height};
        SetZoneRect(ZONE_GAS_STATION_CAR_WINDOW_FRONT, &frontWindowRect);

    }
    SDL_Rect rect = {0, 107, 312, 69};
    SDL_RenderCopy(renderer, maskedBackgroundTexture.get(), NULL, &rect);

    DrawDebugZones();
}

void GasStation::UpdateDebugInfo() {
    if (!debugMode) return;
    
    CarScene::UpdateDebugInfo();
        
    if (debugOverlay) {
        debugOverlay->Clear();
        
        debugOverlay->AddLine("Car X: " + std::to_string(carx) + " Y: " + std::to_string(cary));        

        if (carBitmap != nullptr) {
            CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
            CarClickBoxes_t clickBoxes = metadata.clickBoxes;
            
            debugOverlay->AddLine("cap x=" + std::to_string(clickBoxes.clickBoxGasCap.x) + 
                                " y=" + std::to_string(clickBoxes.clickBoxGasCap.y) +
                                " w=" + std::to_string(clickBoxes.clickBoxGasCap.width) +
                                " h=" + std::to_string(clickBoxes.clickBoxGasCap.height));

            debugOverlay->AddLine("rear: x=" + std::to_string(clickBoxes.clickBoxRearWindow.x) + 
                                " y=" + std::to_string(clickBoxes.clickBoxRearWindow.y) +
                                " w=" + std::to_string(clickBoxes.clickBoxRearWindow.width) +
                                " h=" + std::to_string(clickBoxes.clickBoxRearWindow.height));

            debugOverlay->AddLine("front: x=" + std::to_string(clickBoxes.clickBoxFrontWindow.x) + 
                                " y=" + std::to_string(clickBoxes.clickBoxFrontWindow.y) +
                                " w=" + std::to_string(clickBoxes.clickBoxFrontWindow.width) +
                                " h=" + std::to_string(clickBoxes.clickBoxFrontWindow.height));
        }
        
    }
}

void GasStation::StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;

    if (car != nullptr && carBitmap == nullptr) {
        carBitmap = new CarBitmap(car, nullptr, car->GetColor());
        SDL_Point offscreenPos = GetCarOffscreenPosition();
        carx = offscreenPos.x;
        //cary = offscreenPos.y;
        animatingCar = true;
        currentAnimationType = animType;
    }
}

void GasStation::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation only once after fade
        if (!animatingCar && !carBitmap) {
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
    }
}
