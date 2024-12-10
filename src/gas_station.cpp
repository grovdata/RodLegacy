#include "gas_station.h"
#include "state.h"
#include "parsers/libp.h"
double carAnimationTimeGasStation = 0;

GasStation::GasStation() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, GUS_GAS_SCREEN}) {
    isPumping = false;
    SDL_Rect bottleRect = {7, 161, 8, 24};
    Zone bottle(ZONE_GAS_STATION_PUMP, strdup("Wanna drink ?"), &bottleRect);
    zones.push_back(bottle);
    t = SDL_GetTicks64();
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
    Scene::Draw();


    Uint64 elapsed = SDL_GetTicks64() - t;


    if (elapsed > 2000 && !animatingCar) {
        // printf("start animate\n");
        StartAnimateCar(state.GetCurrentCar(), nullptr, false);
        animatingCar = true;
    }
    if (carBitmap != nullptr) {
        // printf("Move %d %d\n",carx,cary);
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();
    }
    SDL_Rect rect = {0, 107, 312, 69};

    SDL_RenderCopy(renderer, maskedBackgroundTexture.get(), NULL, &rect);
}

void GasStation::StartAnimateCar(Car* car, Driver* driver, bool keepDriving) {
    if (animatingCar && carBitmap) return;
    // From announcing the driver with text, it should take approx. 8 seconds to
    // drive the car to a halt.
    Car *carToAnimate = state.GetCurrentCar();
    CarModel model = carToAnimate->GetModel();
    // SetStatusBar(strdup(model.name.c_str()), PAL1[5]);
    // SetZoneText(ZONE_DINER_BACKGROUND, strdup(model.name.c_str()));

    if (carToAnimate != nullptr && carBitmap == nullptr) {
        carBitmap = new CarBitmap(carToAnimate, nullptr, carToAnimate->GetColor());
        carTimerID = SDL_AddTimer(16, CarAnimationTimerCallback, this);  // 16 ms interval for about 60 FPS
    }
}

Uint32 GasStation::CarAnimationTimerCallback(Uint32 interval, void* param) {
    GasStation* diner = static_cast<GasStation*>(param);
    carAnimationTimeGasStation += interval / 1000.0;
    const double duration = 2.0;
    // Ease-out function (you can experiment with different easing functions)
    double easedValue = (1 - pow(2, -10 * carAnimationTimeGasStation / duration));
    // printf("easedvalue %f\n",easedValue);
    int carX = static_cast<int>((easedValue * (320 + 50)) - 320);  // Scale to cover the range -320 to 100
    // int carX = (int)(easedValue * 100);
    int carY = 100;
    diner->carx = carX;
    diner->cary = carY;

    if (easedValue >= 0.992) {
        // printf("stop\n");
        diner->animatingCar = false;
        return 0;
    }
    // printf("Carx cary %d %d\n",carX,carY);
    return interval;
}