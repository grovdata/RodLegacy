#include "carbitmap.h"
#include "assets.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

CarShadow::CarShadow(int width) {
    this->width = width;
    this->surface = SDL_CreateRGBSurface(0, width, 4, 32,
        0xFF000000,  // Red mask
        0x00FF0000,  // Green mask
        0x0000FF00,  // Blue mask
        0x000000FF   // Alpha mask
    );
}

CarShadow::~CarShadow() {
}

void CarShadow::Draw() {
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0));  // Fill black
    SDL_Rect clearRect;
    Uint32 transparent = SDL_MapRGBA(surface->format, 0, 0, 0, 0);  // Transparent black
    clearRect = {0, 0, 3, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    clearRect = {width - 3, 0, 3, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    clearRect = {0, 2, 3, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    clearRect = {width - 3, 2, 3, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    clearRect = {0, 3, 6, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    clearRect = {width - 6, 3, 6, 1};
    SDL_FillRect(surface, &clearRect, transparent);
    UpdateFromSurface();
    Bitmap::Draw();
}


CarBitmap::CarBitmap(Car* car, Driver* driver, StandardCarColor color) 
    : car(car)
    , body(&bodyOriginal)
    , shadow(car->GetModel().width)
    , wheels()
    , decal()
    , t(SDL_GetTicks64())
    , offsets{}
    , wheelsYOffset(0)
    , showDriver(driver != nullptr)
    , isRoofChopped(false)
    , lastWheelUpdateTime(0)
    , wheelsVisible(true)
    , currentColor(color)
    , gasCapRemoved(false)
    , pumpingHandleVisible(false) {
    
    CarModel model = car->GetModel();

    // Get reference to the lib file
    byte_vector& libFile = state.assets.GetLibPData();
    
    // Load original body
    vector<uint16_t> carPalette = GetStandardCarColor(color);
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)car->GetModel().index;
    auto* originalAsset = state.assets.GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
    if (originalAsset) {
        bodyOriginal = *originalAsset;  // Copy the bitmap
    }
    
    // Load chopped body if available
    if (CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
        auto* choppedAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)(bodyAssetNo + 1)});
        if (choppedAsset) {
            bodyChopped = *choppedAsset;  // Copy the bitmap
        }
    }

    shadow.SetDimensions(model.width, 4);
    shadow.SetVisible(true);

    // Load wheels
    AmigaLibAsset wheelAssetNo = car->GetTires().assetId;    
    auto* wheelAsset = state.assets.GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, wheelAssetNo});
    if (wheelAsset) {
        wheels = *((Spritesheet*)wheelAsset);  // Copy the spritesheet
        wheels.SelectSprite(0, 1);
        wheels.SetVisible(true);
        wheels.SetDimensions(32, 32);
    }

    LoadCarParts(); 
    UpdatePositions();
    UpdateTextures();
}

void CarBitmap::LoadCarParts() {
    // Load decal if car has one
    if (car->GetDecal() != SrLibCarDecalNone) {
        auto* decalAsset = state.assets.GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, (AmigaLibAsset)car->GetDecal()});
        if (decalAsset) {
            decal = *((Spritesheet*)decalAsset);
        }
    }

    // Load other parts similarly
    auto* gasCapAsset = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, CAR_GAS_CAP});
    if (gasCapAsset) {
        gasCap = *gasCapAsset;
    }
    
    // ... load other parts ...
}

void CarBitmap::UpdatePositions() {
    SDL_Rect bodyRect = bodyOriginal.GetPosition();
    
    // Update wheel positions
    rearWheelPos = {bodyRect.x + offsets.offsetXRearTire, 
                    bodyRect.y + offsets.offsetYTires,
                    32, 32};
    frontWheelPos = {bodyRect.x + offsets.offsetXFrontTire,
                     bodyRect.y + offsets.offsetYTires,
                     32, 32};
                     
    // Update other part positions
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);
    // ... update other positions ...
}

void CarBitmap::Draw() {
    driver.Draw();

    if (shadow.IsVisible()) {
        shadow.Draw();
    }
    
    if (wheelsVisible) {
        wheels.SetPosition(frontWheelPos.x, frontWheelPos.y);
        wheels.Draw();
        wheels.SetPosition(rearWheelPos.x, rearWheelPos.y);
        wheels.Draw();
    }

    if (body) {
        body->Draw();
    }
    
    if (gasCap.IsVisible()) {   
        gasCap.Draw();
    }

    if (hoodScoop.IsVisible()) {
        hoodScoop.Draw();
    }

    if (decal.IsVisible()) {
        decal.Draw();
    }
}

void CarBitmap::ToggleChoppedRoof() {
    
    isRoofChopped = !isRoofChopped;
    // body = isRoofChopped ? bodyChopped : bodyOriginal;
    
    // Update position and visibility
    SDL_Rect pos = bodyOriginal.GetPosition();
    body->SetPosition(pos.x, pos.y);
    body->SetVisible(true);
}

void CarBitmap::SetGasCapRemoved(bool removed) {
    gasCap.SetVisible(removed);
}

void CarBitmap::SetPumpingHandleVisible(bool visible) {
    gasCap.SetVisible(visible);
    pumpingHandle.SetVisible(visible);
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
    if (body) {
        body->SetPosition(p.x, p.y);
        UpdatePositions();  // Update all other parts relative to body
    }
}

void CarBitmap::MoveWheelsYOffset(int offset) {
    wheelsYOffset = offset;
    UpdatePositions();  // Recalculate positions with new wheel offset
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    SDL_Rect bodyRect = body->GetPosition();

    // if (driver != nullptr) {
    //     driver.SetPosition(bodyRect.x + offsets.offsetXDriver, bodyRect.y + 7 /*+ model.driverY*/);
    // }
    decal.SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop.SetPosition(bodyRect.x + offsets.offsetXAirScoop, bodyRect.y + offsets.offsetYAirScoop);
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);
    pumpingHandle.SetPosition(bodyRect.x + offsets.offsetXGasCap - 16, bodyRect.y + offsets.offsetYGasCap - 5);
    shadow.SetPosition(bodyRect.x, bodyRect.y + model.height - 6);
    rearWheelhouse.SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + offsets.offsetYTires);
    frontWheelhouse.SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + offsets.offsetYTires);
    // Update black background positions to match wheelhouses
    rearWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXRearTire - 2, bodyRect.y + bodyRect.h - 32);
    frontWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXFrontTire - 2, bodyRect.y + bodyRect.h - 32);
    UpdateTires();
}

void CarBitmap::UpdateTires() {
    SDL_Rect bodyRect = body->GetPosition();
    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    rearWheelPos = {bodyRect.x + offsets.offsetXRearTire, 
                    bodyRect.y + offsets.offsetYTires,
                    32, 32};
    frontWheelPos = {bodyRect.x + offsets.offsetXFrontTire,
                     bodyRect.y + offsets.offsetYTires,
                     32, 32};
    shadow.SetPosition(bodyRect.x, bodyRect.y + offsets.offsetYTires + 32 - 4);
}

void CarBitmap::UpdateWheelAnimation(float speed) {
    // Only update wheels if there's enough movement and enough time has passed
    Uint64 currentTime = SDL_GetTicks64();
    const Uint64 MIN_FRAME_DELAY = 80;  // Minimum 100ms between frame changes
    
    if (fabs(speed) > 0.1f && (currentTime - lastWheelUpdateTime) >= MIN_FRAME_DELAY) {
        //SDL_Log("Updating wheels");
        wheels.CycleNextSprite();
        lastWheelUpdateTime = currentTime;
    }
}

BitmapOffset_t CarBitmap::GetOffset() {
    return offsets;
}

// void CarBitmap::SetCarYBottom(int y) {
//     // Position the car so the wheels' bottom edge is at y
//     this->position.y = y - (31); // 31 is wheel height
//     UpdateTextures(); // Ensure wheels and other parts are updated with new position
// }

void CarBitmap::UpdateColor(StandardCarColor newColor) {
    currentColor = newColor;
    CarModel model = car->GetModel();
    
    vector<uint16_t> carPalette = GetStandardCarColor(newColor);
    
    // Reload bitmap with new palette
    Asset carAsset = {LibAssetRegular, SrLibFileAmigaLibP, model.index};
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