#include "carbitmap.h"
#include "assets.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

const int wheelSize = 31;

CarShadow::CarShadow(int width) {
    this->width = width;
    this->surface = sdl2::SurfacePtr(SDL_CreateRGBSurface(0, width, 4, 32,
        0xFF000000,  // Red mask
        0x00FF0000,  // Green mask
        0x0000FF00,  // Blue mask
        0x000000FF   // Alpha mask
    ));
}

CarShadow::~CarShadow() {
}

void CarShadow::Draw() {
    SDL_FillRect(surface.get(), NULL, SDL_MapRGB(surface->format, 0, 0, 0));  // Fill black
    SDL_Rect clearRect;
    Uint32 transparent = SDL_MapRGBA(surface->format, 0, 0, 0, 0);  // Transparent black
    clearRect = {0, 0, 3, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
    clearRect = {width - 3, 0, 3, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
    clearRect = {0, 2, 3, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
    clearRect = {width - 3, 2, 3, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
    clearRect = {0, 3, 6, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
    clearRect = {width - 6, 3, 6, 1};
    SDL_FillRect(surface.get(), &clearRect, transparent);
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
    , isRoofChopped(false)
    , isFrontBumperStripped(false)
    , isRearBumperStripped(false)
    , lastWheelUpdateTime(0)
    , wheelsVisible(true)
    , currentColor(color)
    , gasCapRemoved(false)
    , pumpingHandleVisible(false) {
    LoadCarParts(car, driver, color);
}

void CarBitmap::LoadCarParts(Car* car, Driver* driverInfo, StandardCarColor color) {
    this->car = car;
    this->currentColor = color;
    
    CarModel model = car->GetModel();
    byte_vector& libFile = state.GetAssets().GetLibPData();
    
    // Initialize the member variable offsets from the car model
    offsets = model.offsets;
    
    // Load original body
    std::vector<uint16_t> carPalette = GetStandardCarColor(color);
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)car->GetModel().index;
    auto* originalAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
    if (originalAsset) {
        bodyOriginal = *originalAsset;  // Copy the bitmap
    }
    
    // Load chopped body if available
    if (CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
        auto* choppedAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)(bodyAssetNo + 1)}, carPalette);
        if (choppedAsset) {
            bodyChopped = *choppedAsset;  // Copy the bitmap (keeps its natural sprite dimensions)
            // Position it with the chopped roof offset from car model
            SDL_Rect origPos = bodyOriginal.GetPosition();
            bodyChopped.SetPosition(origPos.x + model.offsets.offsetXChoppedRoof, origPos.y);
            // Only visible if roof is already chopped
            bodyChopped.SetVisible(car->IsRoofChopped());
            SDL_Log("Loaded chopped body: %dx%d at (%d,%d) [offset=%d], visible=%d", 
                    bodyChopped.GetPosition().w, bodyChopped.GetPosition().h, 
                    origPos.x + model.offsets.offsetXChoppedRoof, origPos.y,
                    model.offsets.offsetXChoppedRoof, car->IsRoofChopped());
        } else {
            SDL_Log("Failed to load chopped body asset");
        }
    }
    
    // Load "chopped bumper" overlays (these will be composited into body texture when stripped)
    if (CarQuery::SrLibQueryCanStripBumpers(bodyAssetNo)) {
        int rearBumperIndex = CarQuery::SrLibQueryGetChoppedBumberRearIndex(bodyAssetNo);
        int frontBumperIndex = CarQuery::SrLibQueryGetChoppedBumberFrontIndex(bodyAssetNo);
        
        SDL_Log("Loading bumper overlays for car %d: rear=%d, front=%d", bodyAssetNo, rearBumperIndex, frontBumperIndex);

        // These are overlays that will be applied to the body texture
        if (rearBumperIndex > 0) {
            auto* rearBumperAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)rearBumperIndex}, carPalette);
            if (rearBumperAsset) {
                rearBumper = *rearBumperAsset;
                SDL_Log("Loaded rear bumper overlay: %dx%d", rearBumper.GetPosition().w, rearBumper.GetPosition().h);
            }
        } else {
            SDL_Log("Car has no rear bumper to strip");
        }
        
        if (frontBumperIndex > 0) {
            auto* frontBumperAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)frontBumperIndex}, carPalette);
            if (frontBumperAsset) {
                frontBumper = *frontBumperAsset;
                SDL_Log("Loaded front bumper overlay: %dx%d", frontBumper.GetPosition().w, frontBumper.GetPosition().h);
            }
        }
        
        isFrontBumperStripped = car->IsBumpersStripped();
        isRearBumperStripped = car->IsBumpersStripped();
        
        SDL_Log("Car bumpers stripped status on load: %s", car->IsBumpersStripped() ? "YES" : "NO");
        
        // If already stripped, apply the overlays to the body texture
        if (isFrontBumperStripped && body && frontBumperIndex > 0) {
            CarModel model = car->GetModel();
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
            SDL_Log("Applied front bumper overlay on load");
        }
        if (isRearBumperStripped && body && rearBumperIndex > 0) {
            CarModel model = car->GetModel();
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
            SDL_Log("Applied rear bumper overlay on load");
        }
    }

    shadow.SetDimensions(model.width, 4);
    shadow.SetVisible(true);

    if (driverInfo != nullptr) {
        AmigaLibAsset driverAssetNo = (AmigaLibAsset)driverInfo->spriteIndex;
        auto* driverAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, driverAssetNo});
        if (driverAsset) {
            driver = *driverAsset;
            driver.SetVisible(true);
            
        }
    }

    // Load wheels
    AmigaLibAsset wheelAssetNo = car->GetTires().assetId;    
    auto* wheelAsset = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, wheelAssetNo});
    if (wheelAsset) {
        wheels = *((Spritesheet*)wheelAsset);  // Copy the spritesheet
        wheels.SelectSprite(0, 1);
        wheels.SetVisible(true);
        wheels.SetDimensions(wheelSize, wheelSize);
    }

    auto* gasCapAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, CAR_GAS_CAP}, carPalette);
    if (gasCapAsset) {
        gasCap = *gasCapAsset;
        gasCap.SetVisible(false);
    }

    customDecal.SetVisible(false);
    if (car->GetDecal() == SrLibCarDecalCustom) {
        // Load custom PNG sticker
        const std::string& path = car->GetCustomDecalPath();
        if (!path.empty()) {
            customDecal.LoadBitmap(path);
            customDecal.SetDimensions(55, 25);
            customDecal.SetVisible(true);
        }
        decal.SetVisible(false);
    } else if (car->GetDecal() != SrLibCarDecalNone) {

        auto* decalAsset = (Spritesheet*)state.GetAssets().GetCarDecals();
        if (decalAsset) {
            decal = *((Spritesheet*)decalAsset);
            decal.SetVisible(true);
        }

        switch (car->GetDecal())
        {
        case SrLibCarDecalAmigaHotLips:
            decal.SelectSprite(0, 0);
            break;
        case SrLibCarDecalAmigaRanger:
            decal.SelectSprite(0, 2);
            break;
        case SrLibCarDecalAmigaNotBulletProof:
            decal.SelectSprite(0, 3);
            break;
        case SrLibCarDecalAmigaHotRod:
            decal.SelectSprite(0, 3);
            break;
        case SrLibCarDecalAmigaDestinyCanWait:
            decal.SelectSprite(1, 0);
            break;
        case SrLibCarDecalAmigaLoveBandit:
            decal.SelectSprite(1, 1);
            break;
        case SrLibCarDecalAmigaBigChief:
            decal.SelectSprite(1, 2);
            break;
        case SrLibCarDecalAmigaImNotACop:
            decal.SelectSprite(1, 3);
            break;

        case SrLibCarDecalNone:
        default:
            decal.SetVisible(false);
            break;
        }
    } else {
        decal.SetVisible(false);
    }

    // Make sure we're pointing to the right body
    body = isRoofChopped ? &bodyChopped : &bodyOriginal;

    UpdatePositions();
    UpdateTextures();
}

void CarBitmap::UpdatePositions() {
    // Always use bodyOriginal for position calculations (it's the full car)
    SDL_Rect bodyRect = bodyOriginal.GetPosition();
    
    // Update wheel positions
    rearWheelPos = {bodyRect.x + offsets.offsetXRearTire, 
                    bodyRect.y + offsets.offsetYTires,
                    wheelSize, wheelSize};
    frontWheelPos = {bodyRect.x + offsets.offsetXFrontTire,
                     bodyRect.y + offsets.offsetYTires,
                     wheelSize, wheelSize};
                     
    // Update other part positions
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);
    
    // Note: bumper overlays are positioned in Draw() to match body exactly
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

    // Always draw the original body
    if (bodyOriginal.IsVisible()) {
        bodyOriginal.Draw();
    }
    
    // If roof is chopped, draw the chopped roof piece on top
    if (isRoofChopped && bodyChopped.IsVisible()) {
        bodyChopped.Draw();
    }
    
    // Bumper overlays are now applied directly to body texture, not drawn separately
    
    if (gasCap.IsVisible()) {   
        gasCap.Draw();
    }

    if (hoodScoop.IsVisible()) {
        hoodScoop.Draw();
    }

    if (decal.IsVisible()) {
        decal.Draw();
    }

    if (customDecal.IsVisible()) {
        customDecal.Draw();
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

void CarBitmap::ChopRoof() {
    if (!car || isRoofChopped) return;
    
    CarModel model = car->GetModel();
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
    
    // Check if this car can have its roof chopped
    if (!CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
        SDL_Log("This car cannot have its roof chopped");
        return;
    }
    
    isRoofChopped = true;
    car->SetRoofChopped(true);
    
    // Apply the chopped roof overlay to mask out the original roof area
    if (bodyChopped.GetSurface()) {
        BitmapOffset_t offsets = model.offsets;
        bodyOriginal.ApplyBumperOverlay(&bodyChopped, offsets.offsetXChoppedRoof, 0);
    }
    
    // Show the chopped roof piece on top
    bodyChopped.SetVisible(true);
    
    SDL_Log("Roof chopped - masked original roof and showing chopped overlay");
}

void CarBitmap::RestoreRoof() {
    if (!car || !isRoofChopped) return;
    
    isRoofChopped = false;
    car->SetRoofChopped(false);
    
    // Reload the original body to restore the roof (removes the mask)
    CarModel model = car->GetModel();
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
    byte_vector& libFile = state.GetAssets().GetLibPData();
    std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);
    
    auto* originalAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
    if (originalAsset) {
        SDL_Rect currentPos = bodyOriginal.GetPosition();
        bool currentVis = bodyOriginal.IsVisible();
        
        bodyOriginal = *originalAsset;
        bodyOriginal.SetPosition(currentPos.x, currentPos.y);
        bodyOriginal.SetVisible(currentVis);
        
        // Reapply any stripped bumpers to the fresh body
        if (isFrontBumperStripped && frontBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            bodyOriginal.ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        }
        if (isRearBumperStripped && rearBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            bodyOriginal.ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        }
    }
    
    // Hide the chopped roof piece
    bodyChopped.SetVisible(false);
    
    SDL_Log("Roof restored - reloaded original body and reapplied bumpers");
}

void CarBitmap::SetGasCapRemoved(bool removed) {
    gasCap.SetVisible(removed);
}

void CarBitmap::SetPumpingHandleVisible(bool visible) {
    gasCap.SetVisible(visible);
    pumpingHandle.SetVisible(visible);
}

void CarBitmap::Move(SDL_Point p) {
    // Update both bodies so they move together
    bodyOriginal.SetPosition(p.x, p.y);
    
    // Apply chopped roof offset
    CarModel model = car->GetModel();
    bodyChopped.SetPosition(p.x + model.offsets.offsetXChoppedRoof, p.y);
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
    // Always use bodyOriginal as the reference position (not body, which might be bodyChopped)
    SDL_Rect bodyRect = bodyOriginal.GetPosition();
    
    driver.SetPosition(bodyRect.x + offsets.offsetXDriver, bodyRect.y + 7 /*+ model.driverY*/);    
    decal.SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    customDecal.SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop.SetPosition(bodyRect.x + offsets.offsetXAirScoop, bodyRect.y + offsets.offsetYAirScoop);
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);
    pumpingHandle.SetPosition(bodyRect.x + offsets.offsetXGasCap - 16, bodyRect.y + offsets.offsetYGasCap - 5);
    
    // Note: bumper overlays are positioned in Draw() to match body exactly
    shadow.SetPosition(bodyRect.x, bodyRect.y + model.height - 6);
    rearWheelhouse.SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + offsets.offsetYTires);
    frontWheelhouse.SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + offsets.offsetYTires);
    // Update black background positions to match wheelhouses
    rearWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXRearTire - 2, bodyRect.y + bodyRect.h - wheelSize);
    frontWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXFrontTire - 2, bodyRect.y + bodyRect.h - wheelSize);
    UpdateTires();
}

void CarBitmap::UpdateTires() {
    // Always use bodyOriginal as the reference position (not body, which might be bodyChopped)
    SDL_Rect bodyRect = bodyOriginal.GetPosition();
    rearWheelPos = {bodyRect.x + this->offsets.offsetXRearTire, 
                    bodyRect.y + this->offsets.offsetYTires,
                    32, 32};
    frontWheelPos = {bodyRect.x + this->offsets.offsetXFrontTire,
                     bodyRect.y + this->offsets.offsetYTires,
                     32, 32};
    shadow.SetPosition(bodyRect.x, bodyRect.y + this->offsets.offsetYTires + 32 - 4);
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
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
    byte_vector& libFile = state.GetAssets().GetLibPData();
    
    std::vector<uint16_t> carPalette = GetStandardCarColor(newColor);
    
    // Save current state - always use bodyOriginal as the reference position
    SDL_Rect currentPos = bodyOriginal.GetPosition();
    bool currentVis = bodyOriginal.IsVisible();
    
    // Preserve the offsets (they don't change with color)
    BitmapOffset_t savedOffsets = offsets;
    
    // Reload body bitmap with new palette
    auto* bodyAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
    if (bodyAsset) {
        bodyOriginal = *bodyAsset;
        bodyOriginal.SetPosition(currentPos.x, currentPos.y);
        bodyOriginal.SetVisible(currentVis);
    }
    
    // Reload chopped body if available
    if (CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
        auto* choppedAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)(bodyAssetNo + 1)}, carPalette);
        if (choppedAsset) {
            // Store current chopped body dimensions if it exists
            SDL_Rect oldChoppedPos = bodyChopped.GetPosition();
            bodyChopped = *choppedAsset;
            
            // Position chopped body relative to bodyOriginal's position
            bodyChopped.SetPosition(currentPos.x + model.offsets.offsetXChoppedRoof, currentPos.y);
            bodyChopped.SetVisible(isRoofChopped);
            
            // If roof is currently chopped, reapply the mask to the freshly colored body
            if (isRoofChopped) {
                BitmapOffset_t offsets = model.offsets;
                bodyOriginal.ApplyBumperOverlay(&bodyChopped, offsets.offsetXChoppedRoof, 0);
            }
        }
    }
    
    // Reload bumper overlays with new color
    if (CarQuery::SrLibQueryCanStripBumpers(bodyAssetNo)) {
        int rearBumperIndex = CarQuery::SrLibQueryGetChoppedBumberRearIndex(bodyAssetNo);
        int frontBumperIndex = CarQuery::SrLibQueryGetChoppedBumberFrontIndex(bodyAssetNo);
        
        if (rearBumperIndex > 0) {
            auto* rearBumperAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)rearBumperIndex}, carPalette);
            if (rearBumperAsset) {
                rearBumper = *rearBumperAsset;
            }
        }
        
        if (frontBumperIndex > 0) {
            auto* frontBumperAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)frontBumperIndex}, carPalette);
            if (frontBumperAsset) {
                frontBumper = *frontBumperAsset;
            }
        }
    }
    
    // Reapply stripped bumpers to the active body with new colors
    if (isFrontBumperStripped && frontBumper.GetSurface()) {
        BitmapOffset_t offsets = model.offsets;
        bodyOriginal.ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        if (isRoofChopped) {
            bodyChopped.ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        }
    }
    
    if (isRearBumperStripped && rearBumper.GetSurface()) {
        BitmapOffset_t offsets = model.offsets;
        bodyOriginal.ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        if (isRoofChopped) {
            bodyChopped.ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        }
    }
    
    // Update the body pointer to point to the correct bitmap
    body = isRoofChopped ? &bodyChopped : &bodyOriginal;
    
    // Restore the offsets (they were saved at the beginning)
    offsets = savedOffsets;
    
    // Update positions to ensure wheels and other parts are aligned correctly
    UpdatePositions();
    UpdateTextures();  // Also update decal, driver, hoodScoop, etc. positions
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

void CarBitmap::SetShadowVisible(bool visible) {
    shadow.SetVisible(visible);
}

void CarBitmap::StripFrontBumper() {
    isFrontBumperStripped = true;
    if (car && body && frontBumper.GetSurface()) {
        CarModel model = car->GetModel();
        BitmapOffset_t offsets = model.offsets;
        // Apply the overlay directly to the body texture
        body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        car->SetBumpersStripped(true);
        SDL_Log("Front bumper stripped");
    }
}

void CarBitmap::StripRearBumper() {
    isRearBumperStripped = true;
    if (car && body && rearBumper.GetSurface()) {
        CarModel model = car->GetModel();
        BitmapOffset_t offsets = model.offsets;
        // Apply the overlay directly to the body texture (no X offset in data)
        body->ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        car->SetBumpersStripped(true);
        SDL_Log("Rear bumper stripped");
    } else if (!rearBumper.GetSurface()) {
        SDL_Log("Cannot strip rear bumper: No rear bumper overlay loaded (car may not have one)");
    }
}

void CarBitmap::RestoreFrontBumper() {
    isFrontBumperStripped = false;
    if (car) {
        // Restore by reloading the body from assets
        CarModel model = car->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
        auto& libFile = state.GetAssets().GetLibPData();
        std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);
        
        if (isRoofChopped) {
            // Chopped body index is body index + 1
            auto* choppedAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)(bodyAssetNo + 1)}, carPalette);
            if (choppedAsset) {
                bodyChopped = *choppedAsset;
                bodyChopped.UpdateFromSurface(); // Update texture after reloading
                body = &bodyChopped;
            }
        } else {
            auto* bodyAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
            if (bodyAsset) {
                bodyOriginal = *bodyAsset;
                bodyOriginal.UpdateFromSurface(); // Update texture after reloading
                body = &bodyOriginal;
            }
        }
        
        // Only set car's bumper state to false if both bumpers are now restored
        if (!isRearBumperStripped) {
            car->SetBumpersStripped(false);
        }
        SDL_Log("Front bumper restored");
        
        // Reapply any remaining stripped bumpers
        if (isRearBumperStripped && rearBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        }
        
        UpdatePositions();
    }
}

void CarBitmap::RestoreRearBumper() {
    isRearBumperStripped = false;
    if (car) {
        // Restore by reloading the body from assets
        CarModel model = car->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;
        auto& libFile = state.GetAssets().GetLibPData();
        std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);
        
        if (isRoofChopped) {
            // Chopped body index is body index + 1
            auto* choppedAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)(bodyAssetNo + 1)}, carPalette);
            if (choppedAsset) {
                bodyChopped = *choppedAsset;
                bodyChopped.UpdateFromSurface(); // Update texture after reloading
                body = &bodyChopped;
            }
        } else {
            auto* bodyAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, bodyAssetNo}, carPalette);
            if (bodyAsset) {
                bodyOriginal = *bodyAsset;
                bodyOriginal.UpdateFromSurface(); // Update texture after reloading
                body = &bodyOriginal;
            }
        }
        
        // Only set car's bumper state to false if both bumpers are now restored
        if (!isFrontBumperStripped) {
            car->SetBumpersStripped(false);
        }
        SDL_Log("Rear bumper restored");
        
        // Reapply any remaining stripped bumpers
        if (isFrontBumperStripped && frontBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        }
        
        UpdatePositions();
    }
}