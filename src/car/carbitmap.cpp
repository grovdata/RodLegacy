#include "core/log.h"
#include "car/carbitmap.h"
#include "core/assets.h"
#include "core/state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

const int wheelSize = 31;

// Sprites are resolved into CarModel::sprites; only the lib differs per set.
namespace {

int DosWheelFirstSprite(CarWheelType type) {
    switch (type) {
        case BRAND:  return SRSE_LIB2_BRAND_WHEEL_1;
        case RACING: return SRSE_LIB2_RACING_WHEEL_1;
        case STOCK:
        default:     return SRSE_LIB2_CAR_WHEEL_1;
    }
}
constexpr int kDosWheelFrames = 4;

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

    const SrGame game = GetCarSetDefs()[model.carSet].game;
    const bool sr2 = game == SrGame::SR2;
    AmigaLibAsset assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_STOCK : CAR_TIRES_STOCK;
    switch (type) {
        case BRAND:  assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_BRAND : CAR_TIRES_BRAND;   break;
        case RACING: assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_RACING : CAR_TIRES_RACING; break;
        default: break;
    }
    auto* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, assetNo, game});
    if (sheet) {
        wheel.SetSpritesheet(*static_cast<AnimatedBitmap*>(sheet), wheelSize, wheelSize);
    }
}

Bitmap* LoadWheelhouse(const CarModel& model, const std::vector<uint16_t>& carPalette) {
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        return state.GetAssets().GetCarSetSpriteWithPalette(
            model.carSet, SRSE_LIB2_WHEELHOUSE, carPalette);
    }
    const SrGame game = GetCarSetDefs()[model.carSet].game;
    byte_vector& libFile = state.GetAssets().GetLibPData(game);
    const int id = game == SrGame::SR2 ? SR2_CAR_WHEELHOUSE : CAR_WHEELHOUSE;
    return state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, id, game}, carPalette);
}

}

Bitmap* LoadCarModelSprite(const CarModel& model, int spriteIndex,
                           const std::vector<uint16_t>& carPalette) {
    if (spriteIndex < 0) return nullptr;
    if (GetCarSetDefs()[model.carSet].art == CarSetArt::DosLib) {
        return state.GetAssets().GetCarSetSpriteWithPalette(model.carSet, spriteIndex, carPalette);
    }
    const SrGame from = GetCarSetDefs()[model.carSet].game;
    byte_vector& libFile = state.GetAssets().GetLibPData(from);
    return state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, spriteIndex, from}, carPalette);
}

Bitmap* LoadDriverPortrait(const Driver& driver) {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    const int set = (driver.driverSet >= 0 && driver.driverSet < (int)defs.size())
                        ? driver.driverSet : kAmigaCarSet;
    return state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, driver.spriteIndex, defs[set].game});
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

    const SrGame game = GetCarSetDefs()[model.carSet].game;
    const bool sr2 = game == SrGame::SR2;
    AmigaLibAsset assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_STOCK : CAR_TIRES_STOCK;
    switch (type) {
        case BRAND:  assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_BRAND : CAR_TIRES_BRAND;   break;
        case RACING: assetNo = sr2 ? (AmigaLibAsset)SR2_CAR_TIRES_RACING : CAR_TIRES_RACING; break;
        default: break;
    }
    Bitmap* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, assetNo, game});
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
        0xFF000000,
        0x00FF0000,
        0x0000FF00,
        0x000000FF
    ));
}

CarShadow::~CarShadow() {
}

void CarShadow::Draw() {
    SDL_FillRect(surface.get(), NULL, SDL_MapRGB(surface->format, 0, 0, 0));
    SDL_Rect clearRect;
    Uint32 transparent = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
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
    , t(GameTicks())
    , offsets{}
    , wheelsYOffset(0)
    , isRoofChopped(false)
    , isFrontBumperStripped(false)
    , isRearBumperStripped(false)
    , wheelsVisible(true)
    , currentColor(color)
    , gasCapRemoved(false) {
    LoadCarParts(car, driver, color);
}

void CarBitmap::LoadCarParts(CarPtr car, Driver* driverInfo, StandardCarColor color) {
    this->car = car;
    this->currentColor = color;

    CarModel model = car->GetModel();
    byte_vector& libFile = state.GetAssets().GetLibPData();

    offsets = model.offsets;

    std::vector<uint16_t> carPalette = GetStandardCarColor(color);
    auto* originalAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
    if (originalAsset) {
        bodyOriginal = *originalAsset;
        bodyOriginal.SetVisible(true);  // Cached asset may have been hidden by a dialog
    }

    const int choppedRoofIndex = model.sprites.choppedRoof;
    if (choppedRoofIndex >= 0) {
        auto* choppedAsset = LoadCarModelSprite(model, choppedRoofIndex, carPalette);
        if (choppedAsset) {
            bodyChopped = *choppedAsset;
            SDL_Rect origPos = bodyOriginal.GetPosition();
            bodyChopped.SetPosition(origPos.x + model.offsets.offsetXChoppedRoof, origPos.y);
            isRoofChopped = car->IsRoofChopped();
            bodyChopped.SetVisible(isRoofChopped);
            if (isRoofChopped) {
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

    const int rearBumperIndex = model.sprites.rearBumperStripped;
    const int frontBumperIndex = model.sprites.frontBumperStripped;
    if (rearBumperIndex >= 0 || frontBumperIndex >= 0) {
        LOG_DEBUG("Loading bumper overlays for car %d: rear=%d, front=%d", model.index, rearBumperIndex, frontBumperIndex);

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
            auto* driverAsset = LoadDriverPortrait(*driverInfo);
            if (driverAsset) {
                driver = *driverAsset;
                driver.SetVisible(true);
            }
        }
    }

    auto* wheelhouseAsset = LoadWheelhouse(model, carPalette);
    if (wheelhouseAsset) {
        rearWheelhouse = *wheelhouseAsset;
        rearWheelhouse.SetVisible(true);
        frontWheelhouse = *wheelhouseAsset;
        frontWheelhouse.SetVisible(true);
    }

    LoadWheelForModel(model, car->GetWheelType(), wheels);

    auto* scoopAsset = LoadCarModelSprite(model, model.sprites.airScoop, carPalette);
    if (scoopAsset) {
        hoodScoop = *scoopAsset;
    }
    hoodScoop.SetVisible(scoopAsset != nullptr);

    const SrGame capGame = GetCarSetDefs()[model.carSet].game;
    const int capId = capGame == SrGame::SR2 ? SR2_CAR_GAS_CAP : CAR_GAS_CAP;
    auto* gasCapAsset = state.GetAssets().GetBitmapAssetWithPalette(
        state.GetAssets().GetLibPData(capGame), {LibAssetRegular, SrLibFileAmigaLibP, capId, capGame}, carPalette);
    if (gasCapAsset) {
        gasCap = *gasCapAsset;
        gasCap.SetVisible(false);
    }

    customDecal.SetVisible(false);
    if (car->GetDecal() == SrLibCarDecalCustom) {
        const std::string& path = car->GetCustomDecalPath();
        if (!path.empty()) {
            customDecal.LoadBitmap(path);
            customDecal.SetVisible(true);
        }
        decal.SetVisible(false);
    } else if (car->GetDecal() >= SrLibCarDecalDosSeDecal1 && car->GetDecal() <= SrLibCarDecalDosSeDecal8) {
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

    body = isRoofChopped ? &bodyChopped : &bodyOriginal;

    UpdatePositions();
    UpdateTextures();
}

void CarBitmap::UpdatePositions() {
    SDL_Rect bodyRect = bodyOriginal.GetPosition();

    // wheelsYOffset is CarDebug's live nudge.
    const int tiresY = bodyRect.y + offsets.offsetYTires + wheelsYOffset;
    const int wheelW = wheels.IsLoaded() ? wheels.Width() : wheelSize;
    const int wheelH = wheels.IsLoaded() ? wheels.Height() : wheelSize;
    rearWheelPos = {bodyRect.x + offsets.offsetXRearTire, tiresY, wheelW, wheelH};
    frontWheelPos = {bodyRect.x + offsets.offsetXFrontTire, tiresY, wheelW, wheelH};

    // Off the wheel cell height: lib.p cells are 31 tall, DOS ones 28.
    shadow.SetPosition(bodyRect.x, tiresY + wheelH - 3);

    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);
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

    if (bodyOriginal.IsVisible()) {
        bodyOriginal.Draw();
    }

    if (isRoofChopped && bodyChopped.IsVisible()) {
        bodyChopped.Draw();
    }

    if (gasCap.IsVisible()) {
        gasCap.Draw();
    }

    if (hoodScoop.IsVisible() && car && car->HasScoop()) {
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

    SDL_Rect pos = bodyOriginal.GetPosition();
    body->SetPosition(pos.x, pos.y);
    body->SetVisible(true);
}

void CarBitmap::ChopRoof() {
    if (!car || isRoofChopped) return;

    CarModel model = car->GetModel();
    AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;

    if (!CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
        LOG_INFO("This car cannot have its roof chopped");
        return;
    }

    isRoofChopped = true;
    car->SetRoofChopped(true);

    if (bodyChopped.GetSurface()) {
        BitmapOffset_t offsets = model.offsets;
        bodyOriginal.ApplyBumperOverlay(&bodyChopped, offsets.offsetXChoppedRoof, 0);
    }

    bodyChopped.SetVisible(true);

    LOG_INFO("Roof chopped - masked original roof and showing chopped overlay");
}

void CarBitmap::RestoreRoof() {
    if (!car || !isRoofChopped) return;

    isRoofChopped = false;
    car->SetRoofChopped(false);

    CarModel model = car->GetModel();
    std::vector<uint16_t> carPalette = GetStandardCarColor(currentColor);

    auto* originalAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
    if (originalAsset) {
        SDL_Rect currentPos = bodyOriginal.GetPosition();
        bool currentVis = bodyOriginal.IsVisible();

        bodyOriginal = *originalAsset;
        bodyOriginal.SetPosition(currentPos.x, currentPos.y);
        bodyOriginal.SetVisible(currentVis);

        if (isFrontBumperStripped && frontBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            bodyOriginal.ApplyBumperOverlay(&frontBumper, offsets.offsetXFrontBumper, offsets.offsetYFrontBumper);
        }
        if (isRearBumperStripped && rearBumper.GetSurface()) {
            BitmapOffset_t offsets = model.offsets;
            bodyOriginal.ApplyBumperOverlay(&rearBumper, 0, offsets.offsetYRearBumper);
        }
    }

    bodyChopped.SetVisible(false);

    LOG_INFO("Roof restored - reloaded original body and reapplied bumpers");
}

void CarBitmap::SetGasCapRemoved(bool removed) {
    gasCap.SetVisible(removed);
}

void CarBitmap::Move(SDL_Point p) {
    bodyOriginal.SetPosition(p.x, p.y);

    CarModel model = car->GetModel();
    bodyChopped.SetPosition(p.x + model.offsets.offsetXChoppedRoof, p.y);
    UpdateTextures();
}

void CarBitmap::MoveBody(SDL_Point p) {
    if (body) {
        body->SetPosition(p.x, p.y);
        UpdatePositions();
    }
}

void CarBitmap::MoveDriverOffset(int dx, int dy) {
    driverOffset.x += dx;
    driverOffset.y += dy;
    UpdateTextures();
}

SDL_Point CarBitmap::GetDriverOffsetFromBody() {
    const int top = offsets.offsetYDriver - driver.GetNaturalSize().h;
    return {offsets.offsetXDriver + driverOffset.x, top + driverOffset.y};
}

void CarBitmap::MoveWheelsYOffset(int offset) {
    wheelsYOffset = offset;
    UpdatePositions();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    SDL_Rect bodyRect = bodyOriginal.GetPosition();

    // y_driver is the portrait's bottom edge relative to the body top.
    driver.SetPosition(bodyRect.x + offsets.offsetXDriver + driverOffset.x,
                       bodyRect.y + offsets.offsetYDriver - driver.GetNaturalSize().h +
                           driverOffset.y);
    decal.SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    customDecal.SetPosition(bodyRect.x + offsets.offsetXDecal, bodyRect.y + offsets.offsetYDecal);
    hoodScoop.SetPosition(bodyRect.x + offsets.offsetXAirScoop, bodyRect.y + offsets.offsetYAirScoop);
    gasCap.SetPosition(bodyRect.x + offsets.offsetXGasCap, bodyRect.y + offsets.offsetYGasCap);

    rearWheelhouse.SetPosition(bodyRect.x + offsets.offsetXRearTire, bodyRect.y + offsets.offsetYTires);
    frontWheelhouse.SetPosition(bodyRect.x + offsets.offsetXFrontTire, bodyRect.y + offsets.offsetYTires);
    rearWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXRearTire - 2, bodyRect.y + bodyRect.h - wheelSize);
    frontWheelhouseBackground.SetPosition(bodyRect.x + offsets.offsetXFrontTire - 2, bodyRect.y + bodyRect.h - wheelSize);
    UpdatePositions();
}

void CarBitmap::UpdateWheelRotation(int carX) {
    // Off the car's X, never a clock: one tire turn is 8 px travelled, and reversing walks the frames back.
    wheels.SelectFrame(carX >> 1);
}

BitmapOffset_t CarBitmap::GetOffset() {
    return offsets;
}

void CarBitmap::UpdateColor(StandardCarColor newColor) {
    currentColor = newColor;
    CarModel model = car->GetModel();

    std::vector<uint16_t> carPalette = GetStandardCarColor(newColor);

    SDL_Rect currentPos = bodyOriginal.GetPosition();
    bool currentVis = bodyOriginal.IsVisible();

    BitmapOffset_t savedOffsets = offsets;

    auto* bodyAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
    if (bodyAsset) {
        bodyOriginal = *bodyAsset;
        bodyOriginal.SetPosition(currentPos.x, currentPos.y);
        bodyOriginal.SetVisible(currentVis);
    }

    const int choppedRoofIndex = model.sprites.choppedRoof;
    if (choppedRoofIndex >= 0) {
        auto* choppedAsset = LoadCarModelSprite(model, choppedRoofIndex, carPalette);
        if (choppedAsset) {
            bodyChopped = *choppedAsset;

            bodyChopped.SetPosition(currentPos.x + model.offsets.offsetXChoppedRoof, currentPos.y);
            bodyChopped.SetVisible(isRoofChopped);

            if (isRoofChopped) {
                BitmapOffset_t offsets = model.offsets;
                bodyOriginal.ApplyBumperOverlay(&bodyChopped, offsets.offsetXChoppedRoof, 0);
            }
        }
    }

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

    auto* scoopAsset = LoadCarModelSprite(model, model.sprites.airScoop, carPalette);
    if (scoopAsset) {
        hoodScoop = *scoopAsset;
        hoodScoop.SetVisible(true);
    }

    auto* wheelhouseAsset = LoadWheelhouse(model, carPalette);
    if (wheelhouseAsset) {
        bool rearVis = rearWheelhouse.IsVisible();
        bool frontVis = frontWheelhouse.IsVisible();
        rearWheelhouse = *wheelhouseAsset;
        rearWheelhouse.SetVisible(rearVis);
        frontWheelhouse = *wheelhouseAsset;
        frontWheelhouse.SetVisible(frontVis);
    }

    body = isRoofChopped ? &bodyChopped : &bodyOriginal;

    offsets = savedOffsets;

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
        Uint64 now = GameTicks();
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
            bodyChopped.UpdateFromSurface();
            body = &bodyChopped;
        }
    } else {
        auto* bodyAsset = LoadCarModelSprite(model, model.sprites.body, carPalette);
        if (bodyAsset) {
            bodyOriginal = *bodyAsset;
            bodyOriginal.UpdateFromSurface();
            body = &bodyOriginal;
        }
    }
}

void CarBitmap::RestoreFrontBumper() {
    isFrontBumperStripped = false;
    if (car) {
        CarModel model = car->GetModel();
        ReloadBodyFromAssets();

        if (!isRearBumperStripped) {
            car->SetBumpersStripped(false);
        }
        LOG_INFO("Front bumper restored");

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
        CarModel model = car->GetModel();
        ReloadBodyFromAssets();

        if (!isFrontBumperStripped) {
            car->SetBumpersStripped(false);
        }
        LOG_INFO("Rear bumper restored");

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

    SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 255, 255, 255));

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