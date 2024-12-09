#include "carbitmap.h"
#include "assets.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

CarBitmap::CarBitmap(Car *car, Driver* driver, StandardCarColor color) {
    this->car = car;
    currentColor = color;
    CarModel model = car->GetModel();
    model.Log();

    offsets = state.carData.GetCarMetadata(model.index).offsets;
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
    // std::cout << "model w: " << static_cast<int>(model.width) << std::endl;
    // std::cout << "model y: " << static_cast<int>(model.height) << std::endl;
    // std::cout << "color: " << static_cast<int>(color) << std::endl;

    // Get car palette for the selected color
    vector<uint16_t> carPalette = GetStandardCarColor(color);
    
    // Load original body
    Asset carAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index};
    bodyOriginal = state.assets.GetBitmapAssetWithPalette(state.assets.GetLibPData(), carAsset, carPalette);
    bodyOriginal->SetPosition(0, 0);
    bodyOriginal->SetDimensions(model.width, model.height);
    bodyOriginal->SetVisible(true);

    // Initialize chopped version
    bodyChopped = nullptr;
    // Set initial state
    isRoofChopped = car->IsRoofChopped();
    body = bodyOriginal;
    
    // Load and prepare chopped version if car can be chopped
    bool canChopRoof = CarQuery::CanChopRoof((AmigaLibAsset)model.index);
    if (canChopRoof && car->IsRoofChopped()) {        
        // Load chopped roof with same palette
        Asset roofAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index+1};
        Bitmap* choppedRoof = state.assets.GetBitmapAssetWithPalette(state.assets.GetLibPData(), roofAsset, carPalette);
        
        // Create new surface for chopped version
        bodyChopped = new Bitmap(*bodyOriginal);  // Create a copy of original body
        
        // Overlay the chopped roof
        OverlayPart(bodyChopped, choppedRoof, offsets.offsetXChoppedRoof, 0);
        body = bodyChopped;
    }

    if (car->IsBumpersStripped()) {
        Asset rearBumperAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index+2};
        Bitmap* rearBumper = state.assets.GetBitmapAssetWithPalette(state.assets.GetLibPData(), rearBumperAsset, carPalette);
        Asset frontBumperAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index+3};
        Bitmap* frontBumper = state.assets.GetBitmapAssetWithPalette(state.assets.GetLibPData(), frontBumperAsset, carPalette);        
        OverlayPart(body, rearBumper, 0, offsets.offsetYRearBumper);
        OverlayPart(body, frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
    }

    
    if (driver != nullptr) {
        showDriver = true;
        int driverSprite = driver->spriteIndex;
        this->driver = state.assets.GetBitmapAsset({LibAssetRegular, AssetLocationAmigaLibP, driverSprite});
        this->driver->SetVisible(true);
    } else {
        showDriver = false;
        //this->driver->SetVisible(false);
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
    delete bodyOriginal;
    if (bodyChopped) {
        delete bodyChopped;
    }
    delete driver;
    delete rearWheel;
    delete frontWheel;
}

void CarBitmap::ToggleChoppedRoof() {
    if (!bodyChopped) return;  // Can't toggle if no chopped version
    
    isRoofChopped = !isRoofChopped;
    body = isRoofChopped ? bodyChopped : bodyOriginal;
    
    // Update position and visibility
    SDL_Rect pos = bodyOriginal->GetPosition();
    body->SetPosition(pos.x, pos.y);
    body->SetVisible(true);
}

void CarBitmap::Draw() {
    // Draw driver first (if visible)
    if (showDriver && driver && driver->IsVisible()) {
        driver->Draw();
    }

    // Draw other parts (wheels, gas cap, etc.)
    if (wheelsVisible) {
        if (rearWheel && rearWheel->IsVisible()) {
            rearWheel->Draw();
        }
        if (frontWheel && frontWheel->IsVisible()) {
            frontWheel->Draw();
        }
    }

    // Draw body on top of driver
    if (body && body->IsVisible()) {
        body->Draw();
    }

    if (gasCap && gasCap->IsVisible()) {
        gasCap->Draw();
    }

    if (hoodScoop && hoodScoop->IsVisible()) {
        hoodScoop->Draw();
    }

    if (decal && decal->IsVisible()) {
        decal->Draw();
    }
}

void CarBitmap::Move(SDL_Point p) {
    if (body) {
        body->SetPosition(p.x, p.y);
    }
    UpdateTextures();
}

// Move the body only, not the driver, wheels, etc.
// Debug purposes
void CarBitmap::MoveBody(SDL_Point p) {
    body->SetPosition(p.x, p.y); 
}

void CarBitmap::MoveWheelsYOffset(int y) {
    this->wheelsYOffset = y;
    UpdateTires();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    SDL_Rect bodyRect = body->GetPosition();
    if (driver != nullptr) {
        driver->SetPosition(bodyRect.x + offsets.offsetXDriver, bodyRect.y + 7 /*+ model.driverY*/);
    }
    gasCap->SetPosition(bodyRect.x /*+ model.gasCapX*/, bodyRect.y/* + model.gasCapY*/);
    decal->SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop->SetPosition(bodyRect.x + offsets.offsetXAirScoop, bodyRect.y + offsets.offsetYAirScoop);
    UpdateTires();
}

void CarBitmap::UpdateTires() {
    SDL_Rect bodyRect = body->GetPosition();
    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    rearWheel->SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + wheelsYOffset);
    frontWheel->SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + wheelsYOffset);
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

BitmapOffset_t CarBitmap::GetOffset() {
    return offsets;
}

void CarBitmap::UpdateColor(StandardCarColor newColor) {
    currentColor = newColor;
    CarModel model = car->GetModel();
    
    vector<uint16_t> carPalette = GetStandardCarColor(newColor);
    
    // Reload bitmap with new palette
    Asset carAsset = {LibAssetRegular, AssetLocationAmigaLibP, model.index};
    bodyOriginal = state.assets.GetBitmapAssetWithPalette(state.assets.GetLibPData(), carAsset, carPalette);
}

void CarBitmap::OverlayPart(Bitmap* baseBitmap, Bitmap* partBitmap, int xOffset, int yOffset) {
    SDL_Surface* baseSurface = baseBitmap->GetSurface();
    SDL_Surface* partSurface = partBitmap->GetSurface();
    
    SDL_Rect partPos = {
        xOffset,
        yOffset,
        partSurface->w,
        partSurface->h
    };
    
    // Clear the area where part will go
    SDL_FillRect(baseSurface, &partPos, SDL_MapRGBA(baseSurface->format, 0, 0, 0, 0));
    
    // Overlay the part
    SDL_BlitSurface(partSurface, NULL, baseSurface, &partPos);
    baseBitmap->UpdateFromSurface();
}