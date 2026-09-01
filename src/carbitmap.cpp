#include "log.h"
#include "carbitmap.h"
#include "assets.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

const int wheelSize = 31;
extern const int kDriverBaseY = 7;

// Where each piece lives is resolved into CarModel::sprites when the model is
// built; only which lib to read from differs per set. Both sources take the
// car's paint, they just apply it differently - see GetDOSCarPalette for the
// DOS side.
namespace {

// The four wheel rotation cells for a tire type in a DOS car set's lib. lib.p
// has one spritesheet asset per type instead; see CarWheel.
int DosWheelFirstSprite(CarWheelType type) {
    switch (type) {
        case BRAND:  return SRSE_LIB2_BRAND_WHEEL_1;
        case RACING: return SRSE_LIB2_RACING_WHEEL_1;
        case STOCK:
        default:     return SRSE_LIB2_CAR_WHEEL_1;
    }
}
constexpr int kDosWheelFrames = 4;

// Loads a car's wheel from whichever lib its set uses.
void LoadWheelForModel(const CarModel& model, CarWheelType type, CarWheel& wheel) {
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        const int first = DosWheelFirstSprite(type);
        std::vector<Bitmap> cells;
        cells.reserve(kDosWheelFrames);
        for (int i = 0; i < kDosWheelFrames; i++) {
            Bitmap* cell = state.GetAssets().GetCarSetSprite(model.carSet, first + i);
            if (!cell) {
                LOG_INFO("Car set wheel sprite %d missing, wheel not loaded", first + i);
                return;
            }
            cells.push_back(*cell);
        }
        wheel.SetFrames(std::move(cells));
        return;
    }

    AmigaLibAsset assetNo = CAR_TIRES_STOCK;
    switch (type) {
        case BRAND:  assetNo = CAR_TIRES_BRAND;  break;
        case RACING: assetNo = CAR_TIRES_RACING; break;
        case STOCK:
        default:     assetNo = CAR_TIRES_STOCK;  break;
    }
    auto* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, assetNo});
    if (sheet) {
        wheel.SetSpritesheet(*static_cast<AnimatedBitmap*>(sheet), wheelSize, wheelSize);
    }
}

// The wheel arch. lib.p has one shared asset; a DOS car set keeps its own.
Bitmap* LoadWheelhouse(const CarModel& model, const std::vector<uint16_t>& carPalette) {
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        return state.GetAssets().GetCarSetSpriteWithPalette(
            model.carSet, SRSE_LIB2_WHEELHOUSE, carPalette);
    }
    byte_vector& libFile = state.GetAssets().GetLibPData();
    return state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, CAR_WHEELHOUSE}, carPalette);
}

} // namespace

Bitmap* LoadCarModelSprite(const CarModel& model, int spriteIndex,
                           const std::vector<uint16_t>& carPalette) {
    if (spriteIndex < 0) return nullptr;
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        return state.GetAssets().GetCarSetSpriteWithPalette(model.carSet, spriteIndex, carPalette);
    }
    byte_vector& libFile = state.GetAssets().GetLibPData();
    return state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, (AmigaLibAsset)spriteIndex}, carPalette);
}

Bitmap* GetCarWheelFrame(const CarModel& model, CarWheelType type, int frame,
                         SDL_Rect* outSourceRect, SDL_Point* outSize) {
    if (outSourceRect) *outSourceRect = {0, 0, 0, 0};
    if (outSize) *outSize = {wheelSize, wheelSize};

    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        const int index = DosWheelFirstSprite(type) + (frame % kDosWheelFrames);
        Bitmap* cell = state.GetAssets().GetCarSetSprite(model.carSet, index);
        if (cell && outSize) {
            SDL_Rect r = cell->GetPosition();
            *outSize = {r.w, r.h};
        }
        return cell;
    }

    AmigaLibAsset assetNo = CAR_TIRES_STOCK;
    switch (type) {
        case BRAND:  assetNo = CAR_TIRES_BRAND;  break;
        case RACING: assetNo = CAR_TIRES_RACING; break;
        case STOCK:
        default:     assetNo = CAR_TIRES_STOCK;  break;
    }
    Bitmap* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, assetNo});
    if (!sheet) return nullptr;

    auto* anim = dynamic_cast<AnimatedBitmap*>(sheet);
    if (anim && outSourceRect && anim->GetColumns() > 0 && anim->GetRows() > 0) {
        const int cellW = sheet->GetSurface()->w / anim->GetColumns();
        const int cellH = sheet->GetSurface()->h / anim->GetRows();
        const int cols = anim->GetColumns();
        *outSourceRect = {(frame % cols) * cellW, 0, cellW, cellH};
    }
    return sheet;
}

void CarWheel::SetSpritesheet(const AnimatedBitmap& source, int drawW, int drawH) {
    sheet = source;
    sheetBacked = true;
    cells.clear();
    frameCount = sheet.GetRows() * sheet.GetColumns();
    if (frameCount <= 0) frameCount = 1;
    frameW = drawW;
    frameH = drawH;
    sheet.SetVisible(true);
    sheet.SetDimensions(frameW, frameH);
    SelectFrame(0);
}

void CarWheel::SetFrames(std::vector<Bitmap> newCells) {
    cells = std::move(newCells);
    sheetBacked = false;
    frameCount = static_cast<int>(cells.size());
    frameW = frameH = 0;
    for (Bitmap& cell : cells) {
        cell.SetVisible(true);
        SDL_Rect r = cell.GetPosition();
        frameW = std::max(frameW, r.w);
        frameH = std::max(frameH, r.h);
    }
    SelectFrame(0);
}

void CarWheel::SelectFrame(int index) {
    if (frameCount <= 0) return;
    current = ((index % frameCount) + frameCount) % frameCount;
    if (sheetBacked) {
        sheet.SelectFrame(current);
    }
}

void CarWheel::CycleNextFrame() { SelectFrame(current + 1); }
void CarWheel::CyclePrevFrame() { SelectFrame(current - 1); }

void CarWheel::DrawAt(int x, int y) {
    if (frameCount <= 0) return;
    if (sheetBacked) {
        sheet.SetPosition(x, y);
        sheet.Draw();
        return;
    }
    cells[current].SetPosition(x, y);
    cells[current].Draw();
}

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


CarBitmap::CarBitmap(CarPtr car, Driver* driver, StandardCarColor color)
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

void CarBitmap::LoadCarParts(CarPtr car, Driver* driverInfo, StandardCarColor color) {
    this->car = car;
    this->currentColor = color;

    CarModel model = car->GetModel();
    byte_vector& libFile = state.GetAssets().GetLibPData();

    // Initialize the member variable offsets from the car model
    offsets = model.offsets;

    // Load original body
    std::vector<uint16_t> carPalette = GetStandardCarColor(color);
    auto* originalAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
    if (originalAsset) {
        bodyOriginal = *originalAsset;  // Copy the bitmap
        bodyOriginal.SetVisible(true);  // Cached asset may have been hidden by a dialog
    }

    // Load chopped body if available
    const int choppedRoofIndex = model.sprites.choppedRoof;
    if (choppedRoofIndex >= 0) {
        auto* choppedAsset = LoadCarModelSprite(model, choppedRoofIndex, carPalette);
        if (choppedAsset) {
            bodyChopped = *choppedAsset;  // Copy the bitmap (keeps its natural sprite dimensions)
            // Position it with the chopped roof offset from car model
            SDL_Rect origPos = bodyOriginal.GetPosition();
            bodyChopped.SetPosition(origPos.x + model.offsets.offsetXChoppedRoof, origPos.y);
            // Only visible if roof is already chopped
            isRoofChopped = car->IsRoofChopped();
            bodyChopped.SetVisible(isRoofChopped);
            if (isRoofChopped) {
                // Mask out the stock roof on the freshly loaded body to match
                bodyOriginal.ApplyBumperOverlay(&bodyChopped, model.offsets.offsetXChoppedRoof, 0);
            }
            LOG_DEBUG("Loaded chopped body: %dx%d at (%d,%d) [offset=%d], visible=%d",
                    bodyChopped.GetPosition().w, bodyChopped.GetPosition().h,
                    origPos.x + model.offsets.offsetXChoppedRoof, origPos.y,
                    model.offsets.offsetXChoppedRoof, car->IsRoofChopped());
        } else {
            LOG_INFO("Failed to load chopped body asset");
        }
    }

    // Load "chopped bumper" overlays (these will be composited into body texture when stripped)
    const int rearBumperIndex = model.sprites.rearBumperStripped;
    const int frontBumperIndex = model.sprites.frontBumperStripped;
    if (rearBumperIndex >= 0 || frontBumperIndex >= 0) {
        LOG_DEBUG("Loading bumper overlays for car %d: rear=%d, front=%d", model.index, rearBumperIndex, frontBumperIndex);

        // These are overlays that will be applied to the body texture
        if (rearBumperIndex >= 0) {
            auto* rearBumperAsset = LoadCarModelSprite(model, rearBumperIndex, carPalette);
            if (rearBumperAsset) {
                rearBumper = *rearBumperAsset;
                LOG_INFO("Loaded rear bumper overlay: %dx%d", rearBumper.GetPosition().w, rearBumper.GetPosition().h);
            }
        } else {
            LOG_INFO("Car has no rear bumper to strip");
        }

        if (frontBumperIndex >= 0) {
            auto* frontBumperAsset = LoadCarModelSprite(model, frontBumperIndex, carPalette);
            if (frontBumperAsset) {
                frontBumper = *frontBumperAsset;
                LOG_INFO("Loaded front bumper overlay: %dx%d", frontBumper.GetPosition().w, frontBumper.GetPosition().h);
            }
        }

        isFrontBumperStripped = car->IsBumpersStripped();
        isRearBumperStripped = car->IsBumpersStripped();

        LOG_INFO("Car bumpers stripped status on load: %s", car->IsBumpersStripped() ? "YES" : "NO");

        // If already stripped, apply the overlays to the body texture
        if (isFrontBumperStripped && body && frontBumperIndex >= 0) {
            CarModel model = car->GetModel();
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
            LOG_INFO("Applied front bumper overlay on load");
        }
        if (isRearBumperStripped && body && rearBumperIndex >= 0) {
            CarModel model = car->GetModel();
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
            LOG_INFO("Applied rear bumper overlay on load");
        }
    }

    shadow.SetDimensions(model.width, 4);
    shadow.SetVisible(true);

    if (driverInfo != nullptr) {
        if (!driverInfo->customDriverPath.empty()) {
            driver.LoadBitmap(driverInfo->customDriverPath);
            driver.SetVisible(true);
        } else {
            AmigaLibAsset driverAssetNo = (AmigaLibAsset)driverInfo->spriteIndex;
            auto* driverAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, driverAssetNo});
            if (driverAsset) {
                driver = *driverAsset;
                driver.SetVisible(true);
            }
        }
    }

    // Load wheelhouse - the arch where wheels sit; wheels overlay on top when equipped
    auto* wheelhouseAsset = LoadWheelhouse(model, carPalette);
    if (wheelhouseAsset) {
        rearWheelhouse = *wheelhouseAsset;
        rearWheelhouse.SetVisible(true);
        frontWheelhouse = *wheelhouseAsset;
        frontWheelhouse.SetVisible(true);
    }

    // Load wheels
    LoadWheelForModel(model, car->GetWheelType(), wheels);

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
            customDecal.SetVisible(true);
        }
        decal.SetVisible(false);
    } else if (car->GetDecal() >= SrLibCarDecalDosSeDecal1 && car->GetDecal() <= SrLibCarDecalDosSeDecal8) {
        // DOS SE decal - load from DOS decal bitmaps
        int dosIndex = car->GetDecal() - SrLibCarDecalDosSeDecal1;
        Bitmap* dosBmp = state.GetAssets().GetDosDecal(dosIndex);
        if (dosBmp) {
            customDecal.LoadSurface(dosBmp->GetSurface());
            customDecal.SetVisible(true);
        }
        decal.SetVisible(false);
    } else if (car->GetDecal() != SrLibCarDecalNone) {

        auto* decalAsset = state.GetAssets().GetCarDecals();
        if (decalAsset) {
            decal = *decalAsset;
            decal.SetVisible(true);
        }

        switch (car->GetDecal())
        {
        case SrLibCarDecalAmigaHotLips:
            decal.SelectFrame(0, 0);
            break;
        case SrLibCarDecalAmigaRanger:
            decal.SelectFrame(0, 1);
            break;
        case SrLibCarDecalAmigaNotBulletProof:
            decal.SelectFrame(0, 2);
            break;
        case SrLibCarDecalAmigaHotRod:
            decal.SelectFrame(0, 3);
            break;
        case SrLibCarDecalAmigaDestinyCanWait:
            decal.SelectFrame(1, 0);
            break;
        case SrLibCarDecalAmigaLoveBandit:
            decal.SelectFrame(1, 1);
            break;
        case SrLibCarDecalAmigaBigChief:
            decal.SelectFrame(1, 2);
            break;
        case SrLibCarDecalAmigaImNotACop:
            decal.SelectFrame(1, 3);
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

    // Update wheel positions. wheelsYOffset is a live nudge on top of the
    // model's offsetYTires, used to measure a car's real wheel line - see
    // CarDebug. It was stored but never applied here, which is why the diner's
    // left/right keys appeared to do nothing.
    const int tiresY = bodyRect.y + offsets.offsetYTires + wheelsYOffset;
    const int wheelW = wheels.IsLoaded() ? wheels.Width() : wheelSize;
    const int wheelH = wheels.IsLoaded() ? wheels.Height() : wheelSize;
    rearWheelPos = {bodyRect.x + offsets.offsetXRearTire, tiresY, wheelW, wheelH};
    frontWheelPos = {bodyRect.x + offsets.offsetXFrontTire, tiresY, wheelW, wheelH};

    // Update other part positions
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);

    // Note: bumper overlays are positioned in Draw() to match body exactly
}

void CarBitmap::Draw() {
    driver.Draw();

    if (shadow.IsVisible()) {
        shadow.Draw();
    }

    if (rearWheelhouse.IsVisible()) {
        rearWheelhouse.Draw();
        frontWheelhouse.Draw();
    }

    if (wheelsVisible) {
        wheels.DrawAt(frontWheelPos.x, frontWheelPos.y);
        wheels.DrawAt(rearWheelPos.x, rearWheelPos.y);
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

    if (stickerWipeActive) {
        SDL_Rect bodyRect = bodyOriginal.GetPosition();
        int decalX = bodyRect.x + offsets.offsetXDecal;
        int decalY = bodyRect.y + offsets.offsetYDecal;
        int totalWidth = 0;
        int decalHeight = 0;
        bool useStockDecal = decal.IsVisible();
        bool useCustomDecal = customDecal.IsVisible();

        if (useStockDecal) {
            SDL_Rect r = decal.GetPosition();
            totalWidth = r.w;
            decalHeight = r.h;
        } else if (useCustomDecal) {
            SDL_Rect r = customDecal.GetPosition();
            totalWidth = r.w;
            decalHeight = r.h;
        }

        if (totalWidth > 0 && decalHeight > 0) {
            int revealedWidth = (int)(stickerWipeProgress * totalWidth);
            if (revealedWidth > totalWidth) revealedWidth = totalWidth;

            SDL_Rect dstRect = {decalX, decalY, revealedWidth, decalHeight};

            if (revealedWidth > 0) {
                if (useStockDecal) {
                    SDL_Rect clip = decal.GetCurrentFrameRect();
                    SDL_Rect srcRect = {clip.x, clip.y, revealedWidth, clip.h};
                    decal.DrawWithRects(srcRect, dstRect);
                } else {
                    SDL_Rect srcRect = {0, 0, revealedWidth, decalHeight};
                    customDecal.DrawWithRects(srcRect, dstRect);
                }
            }

            if (revealedWidth < totalWidth) {
                stickerRoll.SetPosition(decalX + revealedWidth, decalY);
                stickerRoll.Draw();
            }
        }
    } else {
        if (decal.IsVisible()) {
            decal.Draw();
        }

        if (customDecal.IsVisible()) {
            customDecal.Draw();
        }
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
        LOG_INFO("This car cannot have its roof chopped");
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

    LOG_INFO("Roof chopped - masked original roof and showing chopped overlay");
}

void CarBitmap::RestoreRoof() {
    if (!car || !isRoofChopped) return;

    isRoofChopped = false;
    car->SetRoofChopped(false);

    // Reload the original body to restore the roof (removes the mask)
    CarModel model = car->GetModel();
    std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);

    auto* originalAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
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

    LOG_INFO("Roof restored - reloaded original body and reapplied bumpers");
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

void CarBitmap::MoveDriverOffset(int dx, int dy) {
    driverOffset.x += dx;
    driverOffset.y += dy;
    UpdateTextures();
}

SDL_Point CarBitmap::GetDriverOffsetFromBody() const {
    return {offsets.offsetXDriver + driverOffset.x, kDriverBaseY + driverOffset.y};
}

void CarBitmap::MoveWheelsYOffset(int offset) {
    wheelsYOffset = offset;
    UpdatePositions();  // Recalculate positions with new wheel offset
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    // Always use bodyOriginal as the reference position (not body, which might be bodyChopped)
    SDL_Rect bodyRect = bodyOriginal.GetPosition();

    // The driver's X comes from the model, but its Y is this constant for
    // every car. SpriteInfo does carry a per-car y_driver (17..22 across the
    // Amiga cars) which nothing reads - the CarDebug scene is for working out
    // whether that field is what belongs here.
    driver.SetPosition(bodyRect.x + offsets.offsetXDriver + driverOffset.x,
                       bodyRect.y + kDriverBaseY + driverOffset.y);
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
        if (speed < 0) {
            wheels.CyclePrevFrame();
        } else {
            wheels.CycleNextFrame();
        }
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

    std::vector<uint16_t> carPalette = GetStandardCarColor(newColor);

    // Save current state - always use bodyOriginal as the reference position
    SDL_Rect currentPos = bodyOriginal.GetPosition();
    bool currentVis = bodyOriginal.IsVisible();

    // Preserve the offsets (they don't change with color)
    BitmapOffset_t savedOffsets = offsets;

    // Reload body bitmap with new palette
    auto* bodyAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
    if (bodyAsset) {
        bodyOriginal = *bodyAsset;
        bodyOriginal.SetPosition(currentPos.x, currentPos.y);
        bodyOriginal.SetVisible(currentVis);
    }

    // Reload chopped body if available
    const int choppedRoofIndex = model.sprites.choppedRoof;
    if (choppedRoofIndex >= 0) {
        auto* choppedAsset = LoadCarModelSprite(model, choppedRoofIndex, carPalette);
        if (choppedAsset) {
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
    const int rearBumperIndex = model.sprites.rearBumperStripped;
    if (rearBumperIndex >= 0) {
        auto* rearBumperAsset = LoadCarModelSprite(model, rearBumperIndex, carPalette);
        if (rearBumperAsset) {
            rearBumper = *rearBumperAsset;
        }
    }

    const int frontBumperIndex = model.sprites.frontBumperStripped;
    if (frontBumperIndex >= 0) {
        auto* frontBumperAsset = LoadCarModelSprite(model, frontBumperIndex, carPalette);
        if (frontBumperAsset) {
            frontBumper = *frontBumperAsset;
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

    // Reload wheelhouse with new palette
    auto* wheelhouseAsset = LoadWheelhouse(model, carPalette);
    if (wheelhouseAsset) {
        bool rearVis = rearWheelhouse.IsVisible();
        bool frontVis = frontWheelhouse.IsVisible();
        rearWheelhouse = *wheelhouseAsset;
        rearWheelhouse.SetVisible(rearVis);
        frontWheelhouse = *wheelhouseAsset;
        frontWheelhouse.SetVisible(frontVis);
    }

    // Update the body pointer to point to the correct bitmap
    body = isRoofChopped ? &bodyChopped : &bodyOriginal;

    // Restore the offsets (they were saved at the beginning)
    offsets = savedOffsets;

    // Update positions to ensure wheels and other parts are aligned correctly
    UpdatePositions();
    UpdateTextures();  // Also update decal, driver, hoodScoop, etc. positions
}

void CarBitmap::SetShadowVisible(bool visible) {
    shadow.SetVisible(visible);
}

void CarBitmap::SetWheelhousesVisible(bool visible) {
    rearWheelhouse.SetVisible(visible);
    frontWheelhouse.SetVisible(visible);
}

void CarBitmap::DrawWheelAt(int x, int y, bool spin, bool reverse) {
    int totalFrames = wheels.FrameCount();
    if (totalFrames <= 0) return;
    if (spin) {
        Uint64 now = SDL_GetTicks64();
        if (now - lastDrawWheelTime >= 55) {
            if (reverse) {
                rollingFrame = (rollingFrame - 1 + totalFrames) % totalFrames;
            } else {
                rollingFrame = (rollingFrame + 1) % totalFrames;
            }
            lastDrawWheelTime = now;
        }
        wheels.SelectFrame(rollingFrame);
    } else {
        wheels.SelectFrame(0);
    }
    wheels.DrawAt(x, y);
}

void CarBitmap::SetShadowYOffset(int yAdjust) {
    SDL_Rect pos = shadow.GetPosition();
    shadow.SetPosition(pos.x, pos.y + yAdjust);
}

void CarBitmap::LoadNewTireSprite(CarWheelType type) {
    if (!car) return;
    LoadWheelForModel(car->GetModel(), type, wheels);
    UpdatePositions();
}

void CarBitmap::StripFrontBumper() {
    isFrontBumperStripped = true;
    if (car && body && frontBumper.GetSurface()) {
        CarModel model = car->GetModel();
        BitmapOffset_t offsets = model.offsets;
        // Apply the overlay directly to the body texture
        body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        car->SetBumpersStripped(true);
        LOG_INFO("Front bumper stripped");
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
        LOG_INFO("Rear bumper stripped");
    } else if (!rearBumper.GetSurface()) {
        LOG_INFO("Cannot strip rear bumper: No rear bumper overlay loaded (car may not have one)");
    }
}

void CarBitmap::ReloadBodyFromAssets() {
    CarModel model = car->GetModel();
    std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);

    if (isRoofChopped) {
        auto* choppedAsset = LoadCarModelSprite(model, model.sprites.choppedRoof, carPalette);
        if (choppedAsset) {
            bodyChopped = *choppedAsset;
            bodyChopped.UpdateFromSurface(); // Update texture after reloading
            body = &bodyChopped;
        }
    } else {
        auto* bodyAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
        if (bodyAsset) {
            bodyOriginal = *bodyAsset;
            bodyOriginal.UpdateFromSurface(); // Update texture after reloading
            body = &bodyOriginal;
        }
    }
}

void CarBitmap::RestoreFrontBumper() {
    isFrontBumperStripped = false;
    if (car) {
        // Restore by reloading the body from assets
        CarModel model = car->GetModel();
        ReloadBodyFromAssets();

        // Only set car's bumper state to false if both bumpers are now restored
        if (!isRearBumperStripped) {
            car->SetBumpersStripped(false);
        }
        LOG_INFO("Front bumper restored");

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
        ReloadBodyFromAssets();

        // Only set car's bumper state to false if both bumpers are now restored
        if (!isFrontBumperStripped) {
            car->SetBumpersStripped(false);
        }
        LOG_INFO("Rear bumper restored");

        // Reapply any remaining stripped bumpers
        if (isFrontBumperStripped && frontBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            body->ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        }

        UpdatePositions();
    }
}

void CarBitmap::StartStickerWipe() {
    std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);
    if (carPalette.size() < 9) return;

    SDL_Surface* rollSurface = SDL_CreateRGBSurface(0, 5, 25, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!rollSurface) return;

    SDL_PixelFormat* fmt = rollSurface->format;
    auto amigaToRgb = [fmt](uint16_t c) -> Uint32 {
        uint8_t r = ((c >> 8) & 0x0F) * 17;
        uint8_t g = ((c >> 4) & 0x0F) * 17;
        uint8_t b = (c & 0x0F) * 17;
        return SDL_MapRGBA(fmt, r, g, b, 255);
    };

    Uint32 colBright = amigaToRgb(carPalette[8]);
    Uint32 colDark = amigaToRgb(carPalette[0]);
    Uint32 colFixed1 = SDL_MapRGBA(rollSurface->format, 0xa6, 0xb2, 0xb0, 255);
    Uint32 colFixed2 = SDL_MapRGBA(rollSurface->format, 0x90, 0x90, 0x91, 255);

    Uint32 columnColors[5] = { colBright, colFixed1, colFixed2, colDark, colBright };

    if (SDL_MUSTLOCK(rollSurface)) SDL_LockSurface(rollSurface);
    Uint32* pixels = (Uint32*)rollSurface->pixels;
    for (int x = 0; x < 5; x++) {
        Uint32 color = columnColors[x];
        for (int y = 0; y < 25; y++) {
            pixels[y * rollSurface->pitch / 4 + x] = color;
        }
    }
    if (SDL_MUSTLOCK(rollSurface)) SDL_UnlockSurface(rollSurface);

    stickerRoll.LoadSurface(rollSurface);
    SDL_FreeSurface(rollSurface);
    stickerRoll.SetDimensions(5, 25);

    stickerWipeActive = true;
    stickerWipeProgress = 0.0f;
}

void CarBitmap::UpdateStickerWipe(float deltaTime) {
    if (!stickerWipeActive) return;
    stickerWipeProgress += (float)(deltaTime / 0.5);
    if (stickerWipeProgress >= 1.0f) {
        stickerWipeActive = false;
        stickerWipeProgress = 0.0f;
    }
}

SDL_Surface* CarBitmap::CreateColorSwatch(StandardCarColor color, int width, int height) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;

    // White border
    SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 255, 255, 255));

    // Fill interior with gradient from palette entries 1..8
    std::vector<uint16_t> palette = GetStandardCarColor(color);
    int innerW = width - 2;
    int innerH = height - 2;
    for (int x = 0; x < innerW; x++) {
        int palIdx = 1 + x * 8 / innerW;
        if (palIdx > 8) palIdx = 8;
        if (palIdx < static_cast<int>(palette.size())) {
            SrLibColor rgb = BitmapParser::GetRGBFromRGB4(palette[palIdx]);
            SDL_Rect strip = {1 + x, 1, 1, innerH};
            SDL_FillRect(surface, &strip, SDL_MapRGB(surface->format, rgb.r, rgb.g, rgb.b));
        }
    }
    return surface;
}