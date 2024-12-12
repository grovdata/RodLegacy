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

    zones.push_back(pump);
    zones.push_back(rag);

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
    
    initStartTime = SDL_GetTicks64();
}

void GasStation::Action(ZoneId zone) {
    if (carBitmap) carBitmap->SetGasCapRemoved(false);
    if (zone == ZONE_GAS_STATION_PUMP) {
        if (carBitmap) carBitmap->SetGasCapRemoved(true);
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

    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();
    }
    SDL_Rect rect = {0, 107, 312, 69};

    SDL_RenderCopy(renderer, maskedBackgroundTexture.get(), NULL, &rect);
}

void GasStation::StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;

    if (car != nullptr && carBitmap == nullptr) {
        carBitmap = new CarBitmap(car, nullptr, car->GetColor());
        SDL_Point offscreenPos = GetCarOffscreenPosition();
        carx = offscreenPos.x;
        cary = offscreenPos.y;
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
