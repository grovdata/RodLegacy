#include "carbitmap.h"
#include "parsers/libp.h"
#include "assets.h"
#include "state.h"

CarBitmap::CarBitmap(Car *car) {
    this->car = car;
    CarModel model = car->GetModel();

    Asset carAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index};
    body = state.assets.GetBitmapAsset(carAsset);
    body->SetPosition(0, 0);
    body->SetDimensions(model.width, model.height);
    body->SetVisible(true);
    SDL_Rect bodyRect = body->GetPosition();

    driver = state.assets.GetBitmapAsset({LibAssetRegular, AssetLocationAmigaLibP, car->GetDriver()});
    driver->SetVisible(false);

    Asset capAsset = {LibAssetRegular, AssetLocationAmigaLibP, CAR_GAS_CAP};
    gasCap = state.assets.GetBitmapAsset(capAsset);
    gasCap->SetVisible(false);

    Asset scoopAsset = {LibAssetRegular, AssetLocationAmigaLibP, CAR_SCOOP1};
    hoodScoop = state.assets.GetBitmapAsset(scoopAsset);
    hoodScoop->SetPosition(model.width + model.hoodScoopX, model.height + model.hoodScoopY);
    hoodScoop->SetVisible(true);

    AmigaLibAsset wheelAssetNo;
    switch (car->GetTireType())
    {
    case RACING:
        wheelAssetNo = CAR_TIRES_RACING;
    case BRAND:
        wheelAssetNo = CAR_TIRES_BRAND;
    default:
        wheelAssetNo = CAR_TIRES_STOCK;
        break;
    }


    rearWheel = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, wheelAssetNo});
    rearWheel->SelectSprite(1,0);
    frontWheel = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, wheelAssetNo});
    frontWheel->SelectSprite(1,0);

    t = SDL_GetTicks64();
}

CarBitmap::~CarBitmap() {
    delete driver;
    delete rearWheel;
    delete frontWheel;
}


void CarBitmap::Draw() {
    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        rearWheel->CycleNextSprite();
        frontWheel->CycleNextSprite();
    }

    driver->Draw();
    rearWheel->Draw();
    rearWheel->Draw();
    body->Draw();
    gasCap->Draw();
    hoodScoop->Draw();
}

void CarBitmap::Move(SDL_Point p) {
    body->SetPosition(p.x, p.y);
    UpdateTextures();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    SDL_Rect bodyRect = body->GetPosition();
    driver->SetPosition(bodyRect.x + model.driverX, bodyRect.y + model.driverY);
    rearWheel->SetPosition(bodyRect.x + model.rearWheelX, bodyRect.y + model.rearWheelY);
    frontWheel->SetPosition(bodyRect.x + model.frontWheelX, bodyRect.y);

    // rearWheelRect = { bodyRect.x + model.rearWheelX, bodyRect.y + model.rearWheelY, 31, 31};
    // frontWheelRect = { bodyRect.x + model.frontWheelX, bodyRect.y + model.frontWheelY, 31, 31};

    gasCap->SetPosition(bodyRect.x + model.gasCapX, bodyRect.y + model.gasCapY);
}
