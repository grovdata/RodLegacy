#include "carbitmap.h"
#include "assets.h"
#include "state.h"

CarBitmap::CarBitmap(Car *car, Driver* driver) {
    this->car = car;
    CarModel model = car->GetModel();
    model.Log();

    offsets = state.carData.GetCarMetadata(model.index).offsets;
    std::cout << "offsetXRearTire: " << static_cast<int>(offsets.offsetXRearTire) << std::endl;
    std::cout << "offsetXFrontTire: " << static_cast<int>(offsets.offsetXFrontTire) << std::endl;
    std::cout << "offsetXDriver: " << static_cast<int>(offsets.offsetXDriver) << std::endl;
    std::cout << "offsetYDriver: " << static_cast<int>(offsets.offsetYDriver) << std::endl;
    std::cout << "offsetXChoppedRoof: " << static_cast<int>(offsets.offsetXChoppedRoof) << std::endl;
    std::cout << "offsetXAirScoop: " << static_cast<int>(offsets.offsetXAirScoop) << std::endl;
    std::cout << "offsetYAirScoop: " << static_cast<int>(offsets.offsetYAirScoop) << std::endl;
    std::cout << "offsetYRearBumper: " << static_cast<int>(offsets.offsetYRearBumper) << std::endl;
    std::cout << "offsetXFrontBumper: " << static_cast<int>(offsets.offsetXFrontBumper) << std::endl;
    std::cout << "offsetYFrontBumper: " << static_cast<int>(offsets.offsetYFrontBumper) << std::endl;
    std::cout << "offsetXDecal: " << static_cast<int>(offsets.offsetXDecal) << std::endl;
    std::cout << "offsetYDecal: " << static_cast<int>(offsets.offsetYDecal) << std::endl;
    std::cout << "model w: " << static_cast<int>(model.width) << std::endl;
    std::cout << "model y: " << static_cast<int>(model.height) << std::endl;


    Asset carAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index};
    body = state.assets.GetBitmapAsset(carAsset);
    body->SetPosition(0, 0);
    body->SetDimensions(model.width, model.height);
    body->SetVisible(true);
    SDL_Rect bodyRect = body->GetPosition();

    if (driver != nullptr) {
        showDriver = true;
        int driverSprite = driver->spriteIndex;
        this->driver = state.assets.GetBitmapAsset({LibAssetRegular, AssetLocationAmigaLibP, driverSprite});
        this->driver->SetVisible(true);
    } else {
        showDriver = false;
        this->driver->SetVisible(false);
    }

    Asset capAsset = {LibAssetRegular, AssetLocationAmigaLibP, CAR_GAS_CAP};
    gasCap = state.assets.GetBitmapAsset(capAsset);
    gasCap->SetVisible(false);

    CarScoopType scoop = car->GetScoop();
    int scoopAssetId = CAR_SCOOP1;
    switch (scoop) {
        case SCOOP1:
            scoopAssetId = CAR_SCOOP1;
            break;
        case SCOOP2:
            scoopAssetId = CAR_SCOOP2;
            break;
        case SCOOP3:
            scoopAssetId = CAR_SCOOP3;
            break;
        case SCOOP4:
            scoopAssetId = CAR_SCOOP4;
            break;
        case SCOOP_NONE:
            break;
    }

    std::cout << "scoopAssetId: " << scoopAssetId << " hasScoop: " << (scoop != SCOOP_NONE) << std::endl;
    bool hasScoop = scoop != SCOOP_NONE;
    Asset scoopAsset = {LibAssetRegular, AssetLocationAmigaLibP, scoopAssetId};
    hoodScoop = state.assets.GetBitmapAsset(scoopAsset);
    hoodScoop->SetVisible(hasScoop);

    decal = (Spritesheet*)state.assets.GetCarDecals();
    decal->SetVisible(true);

    switch (car->GetDecal())
    {
    case CarDecalHotLips:
        decal->SelectSprite(0, 0);
        break;
    case CarDecalRanger:
        decal->SelectSprite(0, 2);
        break;
    case CarDecalNotBulletProof:
        decal->SelectSprite(0, 3);
        break;
    case CarDecalHotRod:
        decal->SelectSprite(0, 3);
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

    case CarDecalNone:
    default:
        decal->SetVisible(false);
        break;
    }

    //decal->SaveBitmap(DECALS);
    // decal->Print();

    AmigaLibAsset wheelAssetNo = car->GetTires()->GetAssetId();

    rearWheel = (Spritesheet*)state.assets.GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, wheelAssetNo});
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
    if (showDriver)
        driver->Draw();

    rearWheel->Draw();
    frontWheel->Draw();
    body->Draw();
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
    BitmapOffset_t offsets = model.offsets;
    SDL_Rect bodyRect = body->GetPosition();
    driver->SetPosition(bodyRect.x + offsets.offsetXDriver, bodyRect.y + 7 /*+ model.driverY*/);
    rearWheel->SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + 30/* + model.rearWheelY*/);
    frontWheel->SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + 30/* + model.frontWheelY*/);
    gasCap->SetPosition(bodyRect.x /*+ model.gasCapX*/, bodyRect.y/* + model.gasCapY*/);
    decal->SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop->SetPosition(bodyRect.x + offsets.offsetXAirScoop, bodyRect.y + offsets.offsetYAirScoop);
}

void CarBitmap::UpdateWheelAnimation(float speed) {
    // Only update wheels if there's enough movement and enough time has passed
    Uint64 currentTime = SDL_GetTicks64();
    const Uint64 MIN_FRAME_DELAY = 80;  // Minimum 100ms between frame changes
    
    if (fabs(speed) > 0.1f && (currentTime - lastWheelUpdateTime) >= MIN_FRAME_DELAY) {
        rearWheel->CycleNextSprite();
        frontWheel->CycleNextSprite();
        lastWheelUpdateTime = currentTime;
    }
}
