#include "gas_station.h"
#include "state.h"

double carAnimationTimeGasStation = 0;

GasStation::GasStation() : CarScene((Asset){LibAssetRegular, AssetLocationAmigaLibP, GUS_GAS_SCREEN}) {
    // SDL_Rect bottleRect = {7, 161, 8, 24};
    // Zone bottle(ZONE_GARAGE_DRINK, strdup("Wanna drink ?"), &bottleRect);
    // zones.push_back(bottle);
    t = SDL_GetTicks64();
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
    if (zone == ZONE_GARAGE_GOTO_DINER) {
        state.MoveToScene(SCENE_DINER);
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