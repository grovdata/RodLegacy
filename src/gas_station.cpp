#include "gas_station.h"
#include "state.h"
#include "parsers/libp.h"
double carAnimationTimeGasStation = 0;

GasStation::GasStation() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, GUS_GAS_SCREEN}) {
    isPumping = false;
    SDL_Rect bottleRect = {7, 161, 8, 24};
    Zone bottle(ZONE_GAS_STATION_PUMP, strdup("Wanna drink ?"), &bottleRect);
    zones.push_back(bottle);
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

MousePointer GasStation::GetPointerType() {
    return PointerHand;
}

SDL_Point GasStation::GetCarIdlePosition() {
    return {35, 82};
}

SDL_Point GasStation::GetCarOffscreenPosition() {
    return {-320, 82};
}

void GasStation::Action(ZoneId zone) {
    if (zone == ZONE_GAS_STATION_CAR_WINDOW) {
        
    }
}

void GasStation::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    Scene::Draw();

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
        CarScene::StartAnimateCar(car, driver, CarAnimationType::SINGLE_CAR_DRIVE);
    }
}

void GasStation::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation only once after fade
        if (!animatingCar && !carBitmap) {
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
        }
    }
}
