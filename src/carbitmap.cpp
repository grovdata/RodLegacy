#include "carbitmap.h"
#include "assets.h"
#include "state.h"

CarBitmap::CarBitmap(Car *car) {
    this->car = car;
    CarModel model = car->GetModel();

    offsets = state.assets.GetCarOffset(model.index);
    // std::cout << "offsetXRearTire: " << static_cast<int>(offsets.offsetXRearTire) << std::endl;
    // std::cout << "offsetXFrontTire: " << static_cast<int>(offsets.offsetXFrontTire) << std::endl;
    // std::cout << "offsetXDriver: " << static_cast<int>(offsets.offsetXDriver) << std::endl;
    // std::cout << "offsetYDriver: " << static_cast<int>(offsets.offsetYDriver) << std::endl;
    // std::cout << "offsetXChoppedRoof: " << static_cast<int>(offsets.offsetXChoppedRoof) << std::endl;
    // std::cout << "offsetXAirScoop: " << static_cast<int>(offsets.offsetXAirScoop) << std::endl;
    // std::cout << "offsetYAirScoop: " << static_cast<int>(offsets.offsetYAirScoop) << std::endl;
    // std::cout << "offsetYRearBumper: " << static_cast<int>(offsets.offsetYRearBumper) << std::endl;
    // std::cout << "offsetXFrontBumper: " << static_cast<int>(offsets.offsetXFrontBumper) << std::endl;
    // std::cout << "offsetYFrontBumper: " << static_cast<int>(offsets.offsetYFrontBumper) << std::endl;
    // std::cout << "offsetXDecal: " << static_cast<int>(offsets.offsetXDecal) << std::endl;
    // std::cout << "offsetYDecal: " << static_cast<int>(offsets.offsetYDecal) << std::endl;

    Asset carAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index};
    body = state.assets.GetBitmapAsset(carAsset);
    body->SetPosition(0, 0);
    body->SetDimensions(model.width, model.height);
    body->SetVisible(true);
    SDL_Rect bodyRect = body->GetPosition();

    driver = state.assets.GetBitmapAsset({LibAssetRegular, AssetLocationAmigaLibP, car->GetDriver()});
    driver->SetVisible(true);

    Asset capAsset = {LibAssetRegular, AssetLocationAmigaLibP, CAR_GAS_CAP};
    gasCap = state.assets.GetBitmapAsset(capAsset);
    gasCap->SetVisible(false);

    Asset scoopAsset = {LibAssetRegular, AssetLocationAmigaLibP, CAR_SCOOP1};
    hoodScoop = state.assets.GetBitmapAsset(scoopAsset);

    hoodScoop->SetVisible(true);

    decal = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, DECALS});
    SDL_Rect cropDecal = {7, 10, 215, 50};

    // FIXME: Only crop once!
    decal->Crop(cropDecal);

    decal->SetVisible(true);

    switch (car->GetDecal())
    {
    case CarDecalHotLips:
        decal->SelectSprite(0, 1);
        break;
    case CarDecalRanger:
        decal->SelectSprite(0, 2);
        break;
    case CarDecalNotBulletProof:
        decal->SelectSprite(0, 3);
        break;
    case CarDecalHotRod:
        decal->SelectSprite(0, 4);
        break;
    case CarDecalDestinyCanWait:
        decal->SelectSprite(1, 0);
        break;
    case CarDecalLoveBandit:
        decal->SelectSprite(1, 1);
        break;
    case CarDecalBigChief:
        decal->SelectSprite(1, 2);
        break;
    case CarDecalImNotACop:
        decal->SelectSprite(1, 3);
        break;

    default:
        //decal->SetVisible(false);
        break;
    }

    decal->SaveBitmap(DECALS);
    // decal->Print();

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

    rearWheel = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, CAR_TIRES_RACING});
    rearWheel->SelectSprite(0, 1);
    rearWheel->SetVisible(true);
    rearWheel->SetDimensions(31, 31);

    frontWheel = new Spritesheet(1, 4, rearWheel);
    frontWheel->SelectSprite(0, 1);
    frontWheel->SetVisible(true);
    frontWheel->SetDimensions(31, 31);
    UpdateTextures();

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
    body->Draw();
    rearWheel->Draw();
    frontWheel->Draw();
    gasCap->Draw();
    hoodScoop->Draw();
    decal->Draw();
}

void CarBitmap::Move(SDL_Point p) {
    body->SetPosition(p.x, p.y);
    UpdateTextures();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    SDL_Rect bodyRect = body->GetPosition();
    driver->SetPosition(bodyRect.x + offsets.offsetXDriver, bodyRect.y + model.driverY);
    rearWheel->SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + model.rearWheelY);
    frontWheel->SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + model.frontWheelY);
    gasCap->SetPosition(bodyRect.x + model.gasCapX, bodyRect.y + model.gasCapY);
    decal->SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop->SetPosition(bodyRect.x + model.hoodScoopX, bodyRect.y + model.hoodScoopY);
}
