#include "garage/garage.h"
#include "core/state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "core/assets.h"
#include "core/state.h"
#include "car/engine_bay.h"
#include "ui/main_font.h"
#include "core/embedded_loader.h"
#include "audio/audio_manager.h"
#include "car/engine_part.h"
#include "scenes/newspaper.h"
#include "parsers/libp.h"
#include <SDL_image.h>
#include <algorithm>

void Garage::ShowBumperDialog(bool isFront) {
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }

    // A car with no record for the clicked bumper gets message 33 instead of the dialog.
    if (state.GetCurrentCar()) {
        const CarSprites& sprites = state.GetCurrentCar()->GetModelRef().sprites;
        bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
        bool hasBumper = isFront ? sprites.frontBumperStripped >= 0
                                 : sprites.rearBumperStripped >= 0;
        if (!isStripped && !hasBumper) {
            if (bubbles.size() > 6) {
                bubbles[6]->SetVisible(true);  // bumperConstraintBubble
            }
            return;
        }
    }

    bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
    const char* titleText = isFront ? (isStripped ? "Restore bumper - $20" : "Strip bumper - $20")
                                    : (isStripped ? "Restore bumper - $20" : "Strip bumper - $20");

    std::unique_ptr<Dialog>& dialogPtr = isFront ? frontBumperDialog : rearBumperDialog;

    auto onConfirm = [this, isFront, isStripped, &dialogPtr]() {
        if (carBitmap) {
            // Before the operation, so texture loading doesn't delay it.
            if (audioManager) {
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
            }
            if (isStripped) {
                if (isFront) {
                    carBitmap->RestoreFrontBumper();
                    LOG_INFO("Front bumper restored for $20");
                } else {
                    carBitmap->RestoreRearBumper();
                    LOG_INFO("Rear bumper restored for $20");
                }
            } else {
                if (isFront) {
                    carBitmap->StripFrontBumper();
                    LOG_INFO("Front bumper stripped for $20");
                } else {
                    carBitmap->StripRearBumper();
                    LOG_INFO("Rear bumper stripped for $20");
                }
            }
        }

        state.AdvanceGameHours(4);
        dialogPtr->Hide();
        SetActiveDialog(nullptr);
    };

    ShowModificationDialog(titleText, 20, onConfirm, dialogPtr);
}

void Garage::ShowRoofDialog() {
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }

    if (state.GetCurrentCar()) {
        CarModel model = state.GetCurrentCar()->GetModel();

        bool isChopped = carBitmap->IsRoofChopped();
        if (!isChopped && !CarModelCanChopRoof(model)) {
            if (bubbles.size() > 5) {
                bubbles[5]->SetVisible(true);  // roofConstraintBubble
            }
            return;
        }
    }

    bool isChopped = carBitmap->IsRoofChopped();
    const char* titleText = isChopped ? "Restore roof - $70" : "Chop roof - $70";

    auto onConfirm = [this, isChopped]() {
        if (carBitmap) {
            // Before the operation, so texture loading doesn't delay it.
            if (audioManager) {
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
            }
            if (isChopped) {
                carBitmap->RestoreRoof();
                LOG_INFO("Roof restored for $70");
            } else {
                carBitmap->ChopRoof();
                LOG_INFO("Roof chopped for $70");
            }
        }

        state.AdvanceGameHours(16);
        roofDialog->Hide();
        SetActiveDialog(nullptr);
    };

    ShowModificationDialog(titleText, 70, onConfirm, roofDialog);
}

void Garage::CycleNextColor() {
    int nextColor = (previewColor + 1) % CarColorCount;
    previewColor = static_cast<StandardCarColor>(nextColor);
    UpdatePaintCanPreview();
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::CyclePreviousColor() {
    int prevColor = (previewColor - 1 + CarColorCount) % CarColorCount;
    previewColor = static_cast<StandardCarColor>(prevColor);
    UpdatePaintCanPreview();
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::ApplyPreviewColor() {
    if (carBitmap) {
        carBitmap->UpdateColor(previewColor);
        UpdatePaintCanPreview();
    }
}

void Garage::UpdatePaintCanPreview() {
    if (!paintCanPreview) return;

    extern const unsigned char embedded_paintcan_png[];
    extern const size_t embedded_paintcan_png_len;

    SDL_Surface* maskSurface = LoadSurfaceFromMemory(embedded_paintcan_png, embedded_paintcan_png_len);
    if (!maskSurface) {
        LOG_INFO("Failed to load embedded paintcan.png");
        return;
    }

    std::vector<uint16_t> colorPalette = GetStandardCarColor(previewColor);

    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(maskSurface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(maskSurface);

    if (!rgbaSurface) {
        LOG_INFO("Failed to convert paintcan surface");
        return;
    }

    // The mask is grayscale+alpha: opaque brightness maps linearly onto the 9-entry colour ramp.
    SDL_LockSurface(rgbaSurface);
    uint32_t* pixels = static_cast<uint32_t*>(rgbaSurface->pixels);
    int palSize = static_cast<int>(colorPalette.size());

    for (int y = 0; y < rgbaSurface->h; y++) {
        for (int x = 0; x < rgbaSurface->w; x++) {
            int idx = y * rgbaSurface->w + x;

            uint8_t r, g, b, a;
            SDL_GetRGBA(pixels[idx], rgbaSurface->format, &r, &g, &b, &a);

            if (a == 0) continue;

            int brightness = (r + g + b) / 3;
            int paletteIndex = brightness * (palSize - 1) / 255;
            if (paletteIndex >= palSize) paletteIndex = palSize - 1;

            uint16_t amigaColor = colorPalette[paletteIndex];
            uint8_t newR = ((amigaColor >> 8) & 0x0F) * 17;
            uint8_t newG = ((amigaColor >> 4) & 0x0F) * 17;
            uint8_t newB = (amigaColor & 0x0F) * 17;

            pixels[idx] = SDL_MapRGBA(rgbaSurface->format, newR, newG, newB, a);
        }
    }

    SDL_UnlockSurface(rgbaSurface);

    paintCanPreview->ReplaceSurface(rgbaSurface);
}

void Garage::ShowPaintDialog() {
    if (carBitmap) {
        originalColor = carBitmap->GetCurrentColor();
        previewColor = originalColor;
    }

    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }

    UpdatePaintCanPreview();

    SDL_Rect nextColorRect = {149, 127, 78, 16};
    SDL_Rect prevColorRect = {233, 127, 78, 16};
    SDL_Rect goAheadRect = {149, 145, 78, 16};
    SDL_Rect forgetItRect = {233, 145, 78, 16};

    ButtonConfig nextColorButton = {nextColorRect, "Next color", nullptr, false, 0};
    ButtonConfig prevColorButton = {prevColorRect, "Previous color", nullptr, false, 1};
    ButtonConfig goAheadButton = {goAheadRect, "Go ahead", nullptr, true, 2};
    ButtonConfig forgetItButton = {forgetItRect, "Forget it", nullptr, false, 3};
    forgetItButton.cancelChoice = true;

    std::vector<ButtonConfig> buttons = {nextColorButton, prevColorButton, goAheadButton, forgetItButton};

    DialogConfig paintDialogConfig;
    paintDialogConfig.bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT};
    paintDialogConfig.backgroundColor = {0, 0, 0, 0}; // Transparent
    paintDialogConfig.borderColor = {0, 111, 239, 255};
    paintDialogConfig.buttons = buttons;
    paintDialogConfig.contentAreas = {}; // No content areas = no background drawn
    paintDialogConfig.font = state.GetAssets().GetMainFont();
    paintDialogConfig.onButtonPressed = [this](int index) {
        switch (index) {
            case 0: // Next color
                CycleNextColor();
                break;
            case 1: // Previous color
                CyclePreviousColor();
                break;
            case 2: // Go ahead
                {
                    int currentMoney = state.GetBankroll();
                    if (currentMoney >= 20) {
                        state.SetBankroll(currentMoney - 20);
                        const CarPtr& car = state.GetCurrentCarPtr();
                        if (car) {
                            car->SetColor(previewColor);
                            car->SetDecal(SrLibCarDecalNone);  // Remove sticker when repainted
                        }
                        ApplyPreviewColor();
                        if (car) {
                            GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                        }
                        state.AdvanceGameHours(4);
                        LOG_INFO("Car painted for $20");
                        if (bubbles.size() > 3) {
                            bubbles[3]->SetVisible(false);  // Hide paintBubble
                        }
                        paintDialog->Hide();
                        SetActiveDialog(nullptr);
                    } else {
                        LOG_INFO("Not enough money! Need $20");
                        if (bubbles.size() > 3) {
                            bubbles[3]->SetVisible(true);  // paintBubble
                        }
                    }
                }
                break;
            case 3: // Forget it
                previewColor = originalColor;
                UpdatePaintCanPreview();
                if (bubbles.size() > 3) {
                    bubbles[3]->SetVisible(false);  // Hide paintBubble
                }
                paintDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };

    paintDialog = std::make_unique<Dialog>(paintDialogConfig);
    SetActiveDialog(paintDialog.get());
}

void Garage::ShowCarInfoDialog() {
    SDL_Rect dialogRect = {37, 27, 246, 116};

    std::vector<DialogTextConfig> texts;
    texts.push_back(DialogTextConfig{SDL_Rect{dialogRect.x + 3, dialogRect.y + 3, dialogRect.w - 6, 10}, "Car parts", {0xFF, 0xFF, 0xFF, 0xFF}, true});

    std::vector<SDL_Rect> lines = {
        {dialogRect.x + 3, dialogRect.y + 14, dialogRect.w - 6, 1}
    };

    int y = dialogRect.y + 20;
    int xLeft = dialogRect.x + 8;
    int xRightAlign = dialogRect.x + dialogRect.w - 12; // right margin for right-aligned notes

    auto addLeft = [&](const char* s) {
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft, y, dialogRect.w - 16, 10}, s, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };
    auto addIndented = [&](const char* s) {
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft + 10, y, dialogRect.w - 26, 10}, s, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };
    auto addRightNote = [&](const char* left, const char* note) {
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft, y, dialogRect.w - 16, 10}, left, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        int noteWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), note);
        texts.push_back(DialogTextConfig{SDL_Rect{xRightAlign - noteWidth, y, noteWidth, 10}, note, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };

    const CarPtr& car = state.GetCurrentCarPtr();
    if (car) {
        CarModel model = car->GetModel();
        auto vendorToStr = [](CarVendor_t v) -> const char* {
            switch (v) {
                case CarVendorGM: return "GM";
                case CarVendorFord: return "Ford";
                case CarVendorChrysler: return "Chrysler";
                case CarVendorCustom: return "Custom";
                default: return "Unknown";
            }
        };
        auto wearNote = [](int wear) {
            if (WearPercent(wear) <= 0) return std::string("(new)");
            char buf[32];
            snprintf(buf, sizeof(buf), "(%d%% worn)", WearPercent(wear));
            return std::string(buf);
        };
        auto transName = [vendorToStr](AmigaLibAsset aid, CarVendor_t vendor) -> std::string {
            switch ((SrTransmissionType)aid) {
                case SR_TRANS_AUTO_GM: return "Auto Trnsmsn GM.";
                case SR_TRANS_AUTO_FORD: return "Auto Trnsmsn Ford.";
                case SR_TRANS_AUTO_CHRYSLER: return "Auto Trnsmsn Chrysler.";
                case SR_TRANS_3SPD_GM: return "Trnsmsn 3-spd GM.";
                case SR_TRANS_3SPD_FORD: return "Trnsmsn 3-spd Ford.";
                case SR_TRANS_3SPD_CHRYSLER: return "Trnsmsn 3-spd Chrysler.";
                case SR_TRANS_4SPD_GM: return "Trnsmsn 4-spd GM.";
                case SR_TRANS_4SPD_FORD: return "Trnsmsn 4-spd Ford.";
                case SR_TRANS_4SPD_CHRYSLER: return "Trnsmsn 4-spd Chrysler.";
                case SR_TRANS_4SPD_UNIVERSAL: return "Trnsmsn 4-spd Fits all.";
                case SR_TRANS_RACING_GM: return "Racing 4-spd GM.";
                case SR_TRANS_RACING_FORD: return "Racing 4-spd Ford.";
                case SR_TRANS_RACING_CHRYSLER: return "Racing 4-spd Chrysler.";
                default: {
                    std::string v = vendorToStr(vendor);
                    return std::string("Auto Trnsmsn ") + v + ".";
                }
            }
        };
        // Carbs aren't brand-locked, so no vendor prefix.
        auto carbName = [](CarburetorType type) -> const char* {
            switch (type) {
                case CARB_2BARREL: return "2-brl carb. Fits all models.";
                case CARB_4BARREL: return "4-brl carb. Fits all models.";
                case CARB_4BARREL_RACING: return "4-brl racing carb. Fits all models.";
            }
            return "carburetor. Fits all models.";
        };
        // Only the RACING wording is confirmed; the other tiers follow its pattern.
        auto manifoldName = [](ManifoldType type, const char* v) -> std::string {
            switch (type) {
                case MANIFOLD_2BARREL: return std::string(v) + " 2-brl manifold.";
                case MANIFOLD_4BARREL: return std::string(v) + " 4-brl manifold.";
                case MANIFOLD_DUAL_QUAD: return std::string(v) + " dual-quad manifold.";
                case MANIFOLD_TRIPLE_2BARREL: return std::string(v) + " triple 2-brl manifold.";
                case MANIFOLD_RACING: return std::string(v) + " racing manifold.";
            }
            return std::string(v) + " manifold.";
        };
        // Only "Racing slicks." is confirmed.
        auto tireName = [](CarWheelType type) -> const char* {
            switch (type) {
                case STOCK: return "Stock tires.";
                case BRAND: return "Brand name tires.";
                case RACING: return "Racing slicks.";
            }
            return "Tires.";
        };
        auto cylinderLabel = [](CarEngine_t type) -> const char* {
            switch (type) {
                case CarEngineGM_V6:
                case CarEngineFord_V6:
                case CarEngineChrysler_V6:
                    return "V-6";
                default:
                    return "V-8";
            }
        };

        CarVendor_t vendor = model.vendor;
        if (vendor == CarVendorUnknown) {
            vendor = SrLibQueryGetCarVendor(model.index);
        }
        const char* vendorStr = vendorToStr(vendor);

        // GetEngineType(), not the stored EnginePart, which is a placeholder V6 on a spec car. Each line only while
        // the part is fitted: the Part structs survive a removal.
        if (car->HasEngine()) {
            const CarEngine_t engineType = car->GetEngineType();
            const EnginePart engine = car->GetEngine();
            const int displacement = car->GetSpecEngineClass() >= 0
                ? EnginePart(engineType, 0, 0, vendor, ENGINE_V8).displacement
                : engine.displacement;
            char engineBuf[64];
            snprintf(engineBuf, sizeof(engineBuf), "%s %s engine. %d cu.in.",
                     vendorStr, cylinderLabel(engineType), displacement);
            addRightNote(engineBuf, wearNote(engine.wear).c_str());
        }
        if (car->HasManifold()) {
            const Part manifold = car->GetManifold();
            std::string maniText = std::string("  ") + manifoldName(manifold.partType.manifoldType, vendorStr);
            addIndented(maniText.c_str());
        }
        // Dual-quad and triple manifolds carry a set, named as in the classifieds ("2-4brl carbs").
        const int carbCount = car->FittedCarburetorCount();
        if (carbCount > 0) {
            const Part carb = car->GetCarburator();
            std::string carbText = std::string("  ");
            if (carbCount > 1) carbText += std::to_string(carbCount) + "-";
            carbText += carbName(carb.partType.carburetorType);
            if (carbCount > 1) carbText += "s";
            addIndented(carbText.c_str());
        }
        if (car->HasGearbox()) {
            const Part trans = car->GetGearbox();
            std::string transText = transName(trans.assetId, vendor);
            addRightNote(transText.c_str(), wearNote(trans.wear).c_str());
        }
        addRightNote(tireName(car->GetWheelType()), wearNote(car->GetTires().wear).c_str());

        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        char fuelBuf[64];
        snprintf(fuelBuf, sizeof(fuelBuf), "Fuel     %d gallons.", car->GetFuelTenths() / 10);
        addLeft(fuelBuf);
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        // Same number the original prints here.
        char speedBuf[64];
        snprintf(speedBuf, sizeof(speedBuf), "Max speed %d mph.", car->GetTopSpeed());
        addLeft(speedBuf);
    }

    DialogConfig infoCfg;
    infoCfg.bounds = dialogRect;
    infoCfg.backgroundColor = {0, 208, 0, 255};
    infoCfg.borderColor = {0, 111, 239, 255};
    infoCfg.contentAreas = { dialogRect };
    infoCfg.texts = texts;
    infoCfg.lines = lines;
    infoCfg.font = state.GetAssets().GetMainFont();

    // Dismissed by clicking outside or Escape.
    infoCfg.buttons = {};
    infoCfg.onZoneHover = nullptr;
    infoCfg.onButtonPressed = nullptr;
    infoCfg.onDismiss = [this]() {
        carInfoDialog->Hide();
        SetActiveDialog(nullptr);
    };

    carInfoDialog = std::make_unique<Dialog>(infoCfg);
    carInfoDialog->Show();
    SetActiveDialog(carInfoDialog.get());
}

SDL_Point Garage::GetCalendarDateHighlightPosition(int month, int day) {
    const GameClock::CalendarPos pos = GameClock::CalendarHighlightPos(month, day);
    return SDL_Point{pos.x, pos.y};
}

void Garage::ShowCalendarDialog() {
    const GameClock::GameDate today = GameClock::GetDate(state.GetElapsedTicks());
    const int month = today.monthNumber;
    const int day = today.dayOfMonth;

    Bitmap* calBitmap = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP,
         month <= 7 ? CALENDAR_JUNE_JULY_SCREEN : CALENDAR_AUGUST_SEPTEMBER_SCREEN});
    if (!calBitmap) {
        LOG_INFO("Failed to load calendar bitmap");
        return;
    }

    int calW = calBitmap->GetSurface()->w;
    int calH = calBitmap->GetSurface()->h;

    int dialogX = 80;
    int dialogY = 4;
    SDL_Rect dialogRect = {dialogX, dialogY, calW, calH};

    std::map<std::string, DialogBitmap> bitmaps;
    bitmaps["calendar"] = {calBitmap, dialogRect, 0};

    Bitmap* dateRect = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, CALENDAR_CURRENT_DATE_RECTANGLE});
    if (dateRect) {
        int dateW = dateRect->GetSurface()->w;
        int dateH = dateRect->GetSurface()->h;
        // Shift the calendar-relative grid by where the calendar sits.
        SDL_Point datePos = GetCalendarDateHighlightPosition(month, day);
        SDL_Rect dateHighlightRect = {dialogX + datePos.x, dialogY + datePos.y, dateW, dateH};
        bitmaps["date_highlight"] = {dateRect, dateHighlightRect, 1};
    }

    DialogConfig cfg;
    cfg.bounds = dialogRect;
    cfg.backgroundColor = {0, 0, 0, 0};
    cfg.borderColor = {0, 0, 0, 0};
    cfg.bitmaps = bitmaps;
    cfg.font = state.GetAssets().GetMainFont();
    cfg.buttons = {};
    cfg.onDismiss = [this]() {
        calendarDialog->Hide();
        SetActiveDialog(nullptr);
    };

    calendarDialog = std::make_unique<Dialog>(cfg);
    calendarDialog->Show();
    SetActiveDialog(calendarDialog.get());
}

void Garage::LoadCustomStickers() {
    customStickerBitmaps.clear();
    customStickerPaths.clear();

    namespace fs = std::filesystem;
    const fs::path stickerDir = state.GetAssets().GetBasePath() / "stickers";
    LOG_INFO("LoadCustomStickers: looking in %s", stickerDir.string().c_str());
    if (!fs::exists(stickerDir) || !fs::is_directory(stickerDir)) {
        LOG_INFO("LoadCustomStickers: directory not found");
        return;
    }

    std::vector<fs::path> pngFiles;
    for (const auto& entry : fs::directory_iterator(stickerDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".PNG") {
                pngFiles.push_back(entry.path());
            }
        }
    }
    std::sort(pngFiles.begin(), pngFiles.end());

    for (const auto& p : pngFiles) {
        if (customStickerBitmaps.size() >= 16) break;
        auto bmp = std::make_unique<Bitmap>();
        bmp->LoadBitmap(p.string());
        if (bmp->GetSurface()) {
            int w = bmp->GetSurface()->w;
            int h = bmp->GetSurface()->h;
            if (w > 72 || h > 25) {
                LOG_INFO("Skipping custom sticker %s: %dx%d exceeds 72x25 limit", p.string().c_str(), w, h);
                continue;
            }
            customStickerPaths.push_back(p.string());
            customStickerBitmaps.push_back(std::move(bmp));
        } else {
            LOG_INFO("Failed to load custom sticker: %s", p.string().c_str());
        }
    }
    LOG_INFO("Loaded %zu custom stickers from data/stickers/", customStickerBitmaps.size());
}

void Garage::ShowStickerDialog() {
    if (stickerScrollRow == 0) {
        LoadCustomStickers();
    }

    AnimatedBitmap* decalsSheet = state.GetAssets().GetCarDecals();
    if (!decalsSheet) {
        LOG_INFO("ShowStickerDialog: DECALS bitmap not loaded");
        return;
    }

    const SDL_Color frameBg = {0x2A, 0x2A, 0x2A, 0xFF};
    const SDL_Color frameBorder = {0x88, 0x88, 0x88, 0xFF};

    const int SRC_CELL_W = 55;
    const int SRC_CELL_H = 25;
    const int VISIBLE_ROWS = 2;

    int dosDecalCount = state.GetAssets().HasDosDecals() ? 8 : 0;
    int customCount = (int)customStickerBitmaps.size();
    int totalItems = 8 + dosDecalCount + customCount;  // 8 Amiga stock + DOS + custom
    int totalRows = (totalItems + 3) / 4;

    int maxScrollRow = std::max(0, totalRows - VISIBLE_ROWS);
    if (stickerScrollRow > maxScrollRow) stickerScrollRow = maxScrollRow;
    if (stickerScrollRow < 0) stickerScrollRow = 0;

    const int cellW = 264 / 4;
    const int cellH = SRC_CELL_H + 8;
    int dialogH = VISIBLE_ROWS * cellH + 4;

    SDL_Rect dialogRect = {23, 11, 264, dialogH};
    const int GRID_LEFT = dialogRect.x + 2;
    const int GRID_TOP = dialogRect.y + 2;

    std::vector<SDL_Rect> contentAreas = {dialogRect};
    std::map<std::string, DialogBitmap> bitmaps;
    std::vector<Zone> zones;


    // Native size, centred, shrunk only if it does not fit.
    auto fitInCell = [&](const SDL_Rect& cell, int w, int h) {
        if (w > cellW || h > cellH) {
            float scale = std::min((float)cellW / w, (float)cellH / h);
            w = (int)(w * scale);
            h = (int)(h * scale);
        }
        return SDL_Rect{cell.x + (cellW - w) / 2, cell.y + (cellH - h) / 2, w, h};
    };

    // Items 0-7 Amiga stock, 8-15 DOS (if available), then custom.
    for (int displayRow = 0; displayRow < VISIBLE_ROWS; displayRow++) {
        int actualRow = stickerScrollRow + displayRow;
        for (int c = 0; c < 4; c++) {
            int itemIndex = actualRow * 4 + c;
            if (itemIndex >= totalItems) break;

            SDL_Rect dest = {GRID_LEFT + c * cellW, GRID_TOP + displayRow * cellH, cellW, cellH};

            if (itemIndex < 8) {
                int stockRow = itemIndex / 4;
                int stockCol = itemIndex % 4;
                SDL_Rect src = {stockCol * SRC_CELL_W, stockRow * SRC_CELL_H, SRC_CELL_W, SRC_CELL_H};
                DialogBitmap db;
                db.bitmap = decalsSheet;
                db.rect = fitInCell(dest, SRC_CELL_W, SRC_CELL_H);
                db.zOrder = 0;
                db.sourceRect = src;
                bitmaps["decal_" + std::to_string(itemIndex)] = db;
                zones.push_back(Zone("decal_" + std::to_string(itemIndex), SrLibCarDecalNames[itemIndex], dest));
            } else if (dosDecalCount > 0 && itemIndex < 8 + dosDecalCount) {
                int dosIndex = itemIndex - 8;
                Bitmap* dosBmp = state.GetAssets().GetDosDecal(dosIndex);
                if (dosBmp) {
                    SDL_Rect native = dosBmp->GetSourceRect();
                    DialogBitmap db;
                    db.bitmap = dosBmp;
                    db.rect = fitInCell(dest, native.w, native.h);
                    db.zOrder = 0;
                    bitmaps["dos_" + std::to_string(dosIndex)] = db;
                    zones.push_back(Zone("dos_" + std::to_string(dosIndex), SrLibCarDecalNames[8 + dosIndex], dest));
                }
            } else {
                int customIndex = itemIndex - 8 - dosDecalCount;
                Bitmap* customBmp = customStickerBitmaps[customIndex].get();
                SDL_Rect native = customBmp->GetNaturalSize();
                DialogBitmap db;
                db.bitmap = customBmp;
                db.rect = fitInCell(dest, native.w, native.h);
                db.zOrder = 0;
                bitmaps["custom_" + std::to_string(customIndex)] = db;
                std::string stickerName = std::filesystem::path(customStickerPaths[customIndex]).stem().string();
                if (!stickerName.empty()) {
                    std::transform(stickerName.begin(), stickerName.end(), stickerName.begin(), ::tolower);
                    stickerName[0] = std::toupper(stickerName[0]);
                }
                zones.push_back(Zone("custom_" + std::to_string(customIndex), stickerName, dest));
            }
        }
    }

    const int scrollX = 289;
    const int scrollBtnW = 15;
    const int scrollBtnH = 15;
    SDL_Rect scrollTopRect    = {scrollX, dialogRect.y, scrollBtnW, scrollBtnH};
    SDL_Rect scrollUpRect     = {scrollX, dialogRect.y + 16, scrollBtnW, scrollBtnH};
    SDL_Rect scrollDownRect   = {scrollX, dialogRect.y + dialogH - 31, scrollBtnW, scrollBtnH};
    SDL_Rect scrollBottomRect = {scrollX, dialogRect.y + dialogH - 15, scrollBtnW, scrollBtnH};

    zones.push_back(Zone("sticker_scroll_top",    std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect));
    zones.push_back(Zone("sticker_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect));
    zones.push_back(Zone("sticker_scroll_down",   std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect));
    zones.push_back(Zone("sticker_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect));

    std::vector<ButtonConfig> buttons;
    ButtonConfig scrollTopBtn = {scrollTopRect, std::string(1, MAIN_FONT_SCROLL_TOP), nullptr, false, 1};
    scrollTopBtn.fillColor = frameBg;
    scrollTopBtn.borderColor = frameBorder;
    scrollTopBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollUpBtn = {scrollUpRect, std::string(1, MAIN_FONT_SCROLL_UP), nullptr, false, 2};
    scrollUpBtn.fillColor = frameBg;
    scrollUpBtn.borderColor = frameBorder;
    scrollUpBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollDownBtn = {scrollDownRect, std::string(1, MAIN_FONT_SCROLL_DOWN), nullptr, false, 3};
    scrollDownBtn.fillColor = frameBg;
    scrollDownBtn.borderColor = frameBorder;
    scrollDownBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollBottomBtn = {scrollBottomRect, std::string(1, MAIN_FONT_SCROLL_BOTTOM), nullptr, false, 4};
    scrollBottomBtn.fillColor = frameBg;
    scrollBottomBtn.borderColor = frameBorder;
    scrollBottomBtn.textColor = ButtonDefaultChoiceColor;
    buttons = {scrollTopBtn, scrollUpBtn, scrollDownBtn, scrollBottomBtn};

    DialogConfig stickerCfg;
    stickerCfg.bounds = {dialogRect.x, dialogRect.y, (scrollX + scrollBtnW) - dialogRect.x, dialogH};
    stickerCfg.backgroundColor = frameBg;
    stickerCfg.borderColor = frameBorder;
    stickerCfg.contentAreas = contentAreas;
    stickerCfg.bitmaps = bitmaps;
    stickerCfg.zones = zones;
    stickerCfg.buttons = buttons;
    stickerCfg.font = state.GetAssets().GetMainFont();
    stickerCfg.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    stickerCfg.onButtonPressed = [this, maxScrollRow](int index) {
        switch (index) {
            case 1: // TOP
                if (stickerScrollRow > 0) {
                    stickerScrollRow = 0;
                    stickerNeedsRefresh = true;
                }
                break;
            case 2: // UP
                if (stickerScrollRow > 0) {
                    stickerScrollRow--;
                    stickerNeedsRefresh = true;
                }
                break;
            case 3: // DOWN
                if (stickerScrollRow < maxScrollRow) {
                    stickerScrollRow++;
                    stickerNeedsRefresh = true;
                }
                break;
            case 4: // BOTTOM
                if (stickerScrollRow < maxScrollRow) {
                    stickerScrollRow = maxScrollRow;
                    stickerNeedsRefresh = true;
                }
                break;
        }
    };
    stickerCfg.onZoneClick = [this](Zone& zone) {
        const std::string& id = zone.GetId();
        auto applySticker = [&](auto applyFn) {
            const CarPtr& car = state.GetCurrentCarPtr();
            if (car && car->CanAddDecal()) {
                int money = state.GetBankroll();
                if (money >= 5) {
                    state.SetBankroll(money - 5);
                    state.AdvanceGameHours(1);
                    applyFn(car.get());
                    GetOrCreateCarBitmap(car)->LoadCarParts(car, nullptr, car->GetColor());
                    GetOrCreateCarBitmap(car)->StartStickerWipe();
                }
            }
            stickerScrollRow = 0;
            stickerDialog->Hide();
            SetActiveDialog(nullptr);
        };

        if (id.rfind("decal_", 0) == 0) {
            int decalIndex = std::stoi(id.substr(6));
            applySticker([decalIndex](Car* car) {
                car->SetDecal((SrLibCarDecal)decalIndex);
                car->SetCustomDecalPath("");
            });
            return;
        }
        if (id.rfind("dos_", 0) == 0) {
            int dosIndex = std::stoi(id.substr(4));
            applySticker([dosIndex](Car* car) {
                car->SetDecal((SrLibCarDecal)(dosIndex + SrLibCarDecalDosSeDecal1));
                car->SetCustomDecalPath("");
            });
            return;
        }
        if (id.rfind("custom_", 0) == 0) {
            int customIndex = std::stoi(id.substr(7));
            if (customIndex >= 0 && customIndex < (int)customStickerPaths.size()) {
                std::string path = customStickerPaths[customIndex];
                applySticker([path](Car* car) {
                    car->SetDecal(SrLibCarDecalCustom);
                    car->SetCustomDecalPath(path);
                });
            }
            return;
        }
    };
    stickerCfg.onDismiss = [this]() {
        stickerScrollRow = 0;
        stickerDialog->Hide();
        SetActiveDialog(nullptr);
    };

    stickerDialog = std::make_unique<Dialog>(stickerCfg);
    stickerDialog->Show();
    SetActiveDialog(stickerDialog.get());
}

void Garage::ShowModificationDialog(const char* title, int price, std::function<void()> onConfirm, std::unique_ptr<Dialog>& dialogPtr) {
    const int dialogWidth = 156;
    const int dialogHeight = 36;
    const int dialogX = 82;
    const int dialogY = 50;

    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};

    const int buttonWidth = 70;
    const int buttonHeight = 14;
    const int buttonSpacing = 4;
    const int buttonY = dialogY + dialogHeight - buttonHeight - 4;

    SDL_Rect okButtonRect = {dialogX + 4, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetButtonRect = {dialogX + 4 + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight};

    ButtonConfig okButton = {okButtonRect, "OK", nullptr, true, 0}; // OK is default
    ButtonConfig forgetButton = {forgetButtonRect, "Forget it", nullptr, false, 1};
    forgetButton.cancelChoice = true;

    std::vector<ButtonConfig> buttons = {okButton, forgetButton};

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{dialogX + 3, dialogY + 3, dialogWidth, 10}, title, {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {dialogX + 3, dialogY + 14, dialogWidth - 6, 1}
    };

    DialogConfig modDialogConfig;
    modDialogConfig.bounds = dialogRect;
    modDialogConfig.backgroundColor = {0, 208, 0, 255};
    modDialogConfig.borderColor = {0, 111, 239, 255};
    modDialogConfig.texts = texts;
    modDialogConfig.buttons = buttons;
    modDialogConfig.lines = lines;
    modDialogConfig.contentAreas = std::vector<SDL_Rect>{dialogRect};
    modDialogConfig.font = state.GetAssets().GetMainFont();
    modDialogConfig.onButtonPressed = [this, price, onConfirm, &dialogPtr](int index) {
        switch (index) {
            case 0: // OK
                {
                    int currentMoney = state.GetBankroll();
                    if (currentMoney < price) {
                        LOG_INFO("Not enough money! Need $%d", price);
                        if (bubbles.size() > 4) {
                            bubbles[4]->SetVisible(true);  // customizationBubble
                        }
                        return;
                    }

                    state.SetBankroll(currentMoney - price);

                    onConfirm();

                    if (bubbles.size() > 4) {
                        bubbles[4]->SetVisible(false);
                    }
                    dialogPtr->Hide();
                    SetActiveDialog(nullptr);
                }
                break;
            case 1: // Forget it
                if (bubbles.size() > 4) {
                    bubbles[4]->SetVisible(false);
                }
                dialogPtr->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };

    dialogPtr = std::make_unique<Dialog>(modDialogConfig);
    SetActiveDialog(dialogPtr.get());
}

static const char* WheelTypeName(CarWheelType type) {
    switch (type) {
        case STOCK:  return "Stock tires";
        case BRAND:  return "Brand name tires";
        case RACING: return "Racing tires";
        default:     return "Unknown tires";
    }
}

// The newspaper's own ad name first, a generic description for parts never in the catalog.
static std::string TransmissionTypeName(TransmissionType type, CarVendor_t vendor) {
    if (Newspaper* np = state.GetNewspaper()) {
        std::string catalogName = np->FindTransmissionCatalogName(type, vendor);
        if (!catalogName.empty()) return catalogName;
    }
    switch (type) {
        case TRANS_AUTO_GM:        return "Auto transmission (GM)";
        case TRANS_AUTO_FORD:      return "Auto transmission (Ford)";
        case TRANS_AUTO_CHRYSLER:  return "Auto transmission (Chrysler)";
        case TRANS_3SPD_GM:        return "3-speed (GM)";
        case TRANS_3SPD_FORD:      return "3-speed (Ford)";
        case TRANS_3SPD_CHRYSLER:  return "3-speed (Chrysler)";
        case TRANS_4SPD_GM:        return "4-speed (GM)";
        case TRANS_4SPD_FORD:      return "4-speed (Ford)";
        case TRANS_4SPD_CHRYSLER:  return "4-speed (Chrysler)";
        case TRANS_4SPD_UNIVERSAL: return "4-speed (universal)";
        case TRANS_RACING_GM:       return "Racing 4-speed (GM)";
        case TRANS_RACING_FORD:     return "Racing 4-speed (Ford)";
        case TRANS_RACING_CHRYSLER: return "Racing 4-speed (Chrysler)";
        default: return "Unknown transmission";
    }
}

static std::string ManifoldTypeName(ManifoldType type, CarVendor_t vendor) {
    if (Newspaper* np = state.GetNewspaper()) {
        std::string catalogName = np->FindManifoldCatalogName(type, vendor);
        if (!catalogName.empty()) return catalogName;
    }
    switch (type) {
        case MANIFOLD_2BARREL:       return "2-barrel manifold";
        case MANIFOLD_4BARREL:       return "4-barrel manifold";
        case MANIFOLD_DUAL_QUAD:     return "Dual-quad manifold";
        case MANIFOLD_TRIPLE_2BARREL: return "Triple 2-barrel manifold";
        case MANIFOLD_RACING:        return "Racing manifold";
        default: return "Unknown manifold";
    }
}

static std::string CarburetorTypeName(CarburetorType type) {
    if (Newspaper* np = state.GetNewspaper()) {
        std::string catalogName = np->FindCarburetorCatalogName(type);
        if (!catalogName.empty()) return catalogName;
    }
    switch (type) {
        case CARB_2BARREL:        return "2-barrel carburetor";
        case CARB_4BARREL:        return "4-barrel carburetor";
        case CARB_4BARREL_RACING: return "Racing 4-barrel carburetor";
        default: return "Unknown carburetor";
    }
}

static std::string EngineDisplayName(const EnginePart& p) {
    if (Newspaper* np = state.GetNewspaper()) {
        std::string catalogName = np->FindEngineCatalogName(p.engineType, p.vendor);
        if (!catalogName.empty()) return catalogName;
    }
    return p.GetEngineName();
}

static std::string WearNote(int wear) {
    if (WearPercent(wear) <= 0) return "(new)";
    char buf[32];
    snprintf(buf, sizeof(buf), "(%d%% worn)", WearPercent(wear));
    return std::string(buf);
}

void Garage::ShowTransmissionDialog() {
    const auto& spareTransmissions = state.GetSpareTransmissions();
    // Say so rather than swallow the click.
    if (spareTransmissions.empty()) { bubbles[1]->SetVisible(true); return; }

    std::vector<std::string> labels;
    for (const Part& p : spareTransmissions) {
        std::string name = p.name.empty() ? TransmissionTypeName(p.partType.transmissionType, p.vendor) : p.name;
        labels.push_back(name + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {20, 17, 245, 136};

    const int buttonWidth = 66;
    const int buttonHeight = 16;

    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect switchItRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {160, 127, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone switchItZone("trans_switch_it",     "Change", switchItRect);
    Zone forgetItZone("trans_forget_it",     "Forget it", forgetItRect);
    Zone scrollTopZone("trans_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("trans_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("trans_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("trans_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {switchItRect, "Change",    nullptr, true,  0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10}, "Spare transmissions", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1} };

    std::vector<Zone> dialogZones = {
        switchItZone, forgetItZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: {
                if (transInstallPhase != TransInstallPhase::EMPTY) {
                    bubbles[8]->SetVisible(true);
                    break;
                }
                int selectedIndex = transmissionDialog->GetSelectedIndex();
                if (selectedIndex < 0) break;
                const auto& spareTransmissions = state.GetSpareTransmissions();
                if (selectedIndex >= (int)spareTransmissions.size()) break;
                Part newTransmission = spareTransmissions[selectedIndex];
                // The original's only transmission fitment test is make; a mismatch still slides in and bounces back.
                const CarPtr& car = state.GetCurrentCarPtr();
                transRejected = car &&
                    !PartsCompatibilityManager::IsVendorCompatible(newTransmission.vendor,
                                                                   car->GetModelRef().vendor);
                transmissionDialog->Hide();
                // The actual swap happens once the slide-in/screw-down animation completes.
                StartTransmissionSlideIn(newTransmission);
                SetActiveDialog(transmissionInstallDialog.get());
                break;
            }
            case 1: // Forget it
                transmissionDialog->Hide();
                SetActiveDialog(transmissionInstallDialog.get());
                break;
        }
    };
    dialogConfig.texts = texts;

    transmissionDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            {},
            [](int) {},
            listBoxRect,
            selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)transmissionDialog.get());
}

// Only reachable while the slot is already EMPTY, so no "make room first" bubble.
void Garage::ShowEngineDialog() {
    const auto& spareEngines = state.GetSpareEngines();
    if (spareEngines.empty()) { bubbles[9]->SetVisible(true); return; }

    std::vector<std::string> labels;
    for (const EnginePart& p : spareEngines) {
        std::string name = p.name.empty() ? EngineDisplayName(p) : p.name;
        labels.push_back(name + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {20, 17, 245, 136};
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect switchItRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {160, 127, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone switchItZone("engine_switch_it", "Change", switchItRect);
    Zone forgetItZone("engine_forget_it", "Forget it", forgetItRect);
    Zone scrollTopZone("engine_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("engine_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("engine_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("engine_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {switchItRect, "Change",    nullptr, true,  0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10}, "Spare engines", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1} };
    std::vector<Zone> dialogZones = {
        switchItZone, forgetItZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Change
                int selectedIndex = engineSwapDialog->GetSelectedIndex();
                if (selectedIndex < 0) break;
                const auto& spareEngines = state.GetSpareEngines();
                if (selectedIndex >= (int)spareEngines.size()) break;
                EnginePart newEngine = spareEngines[selectedIndex];
                const CarPtr& car = state.GetCurrentCarPtr();
                // For the block, make is the only test.
                engineBaySlotRejected[(int)EngineBaySlot::ENGINE] = car &&
                    !PartsCompatibilityManager::IsVendorCompatible(newEngine.vendor, car->GetModelRef().vendor);
                engineSwapDialog->Hide();
                StartEngineSlideIn(newEngine);
                SetActiveDialog(engineBay->GetDialog());
                break;
            }
            case 1: // Forget it
                engineSwapDialog->Hide();
                SetActiveDialog(engineBay->GetDialog());
                break;
        }
    };
    dialogConfig.texts = texts;

    engineSwapDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels, {}, [](int) {},
            listBoxRect, selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)engineSwapDialog.get());
}

void Garage::ShowManifoldDialog() {
    const auto& spareManifolds = state.GetSpareManifolds();
    if (spareManifolds.empty()) { bubbles[10]->SetVisible(true); return; }

    std::vector<std::string> labels;
    for (const Part& p : spareManifolds) {
        std::string name = p.name.empty() ? ManifoldTypeName(p.partType.manifoldType, p.vendor) : p.name;
        labels.push_back(name + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {20, 17, 245, 136};
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect switchItRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {160, 127, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone switchItZone("manifold_switch_it", "Change", switchItRect);
    Zone forgetItZone("manifold_forget_it", "Forget it", forgetItRect);
    Zone scrollTopZone("manifold_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("manifold_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("manifold_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("manifold_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {switchItRect, "Change",    nullptr, true,  0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10}, "Spare manifolds", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1} };
    std::vector<Zone> dialogZones = {
        switchItZone, forgetItZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Change
                int selectedIndex = manifoldSwapDialog->GetSelectedIndex();
                if (selectedIndex < 0) break;
                const auto& spareManifolds = state.GetSpareManifolds();
                if (selectedIndex >= (int)spareManifolds.size()) break;
                Part newManifold = spareManifolds[selectedIndex];
                const CarPtr& car = state.GetCurrentCarPtr();
                engineBaySlotRejected[(int)EngineBaySlot::MANIFOLD] = car &&
                    (!PartsCompatibilityManager::IsVendorCompatible(newManifold.vendor, car->GetModelRef().vendor) ||
                     !PartsCompatibilityManager::IsManifoldCompatible(newManifold.partType.manifoldType, car->GetEngineType()));
                manifoldSwapDialog->Hide();
                StartManifoldSlideIn(newManifold);
                SetActiveDialog(engineBay->GetDialog());
                break;
            }
            case 1: // Forget it
                manifoldSwapDialog->Hide();
                SetActiveDialog(engineBay->GetDialog());
                break;
        }
    };
    dialogConfig.texts = texts;

    manifoldSwapDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels, {}, [](int) {},
            listBoxRect, selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)manifoldSwapDialog.get());
}

void Garage::ShowCarburetorDialog(int carbIndex) {
    const auto& spareCarburetors = state.GetSpareCarburetors();
    if (spareCarburetors.empty()) { bubbles[11]->SetVisible(true); return; }

    std::vector<std::string> labels;
    for (const Part& p : spareCarburetors) {
        std::string name = p.name.empty() ? CarburetorTypeName(p.partType.carburetorType) : p.name;
        labels.push_back(name + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {20, 17, 245, 136};
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect switchItRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {160, 127, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone switchItZone("carb_switch_it", "Change", switchItRect);
    Zone forgetItZone("carb_forget_it", "Forget it", forgetItRect);
    Zone scrollTopZone("carb_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("carb_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("carb_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("carb_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {switchItRect, "Change",    nullptr, true,  0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10}, "Spare carburetors", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1} };
    std::vector<Zone> dialogZones = {
        switchItZone, forgetItZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this, carbIndex](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Change
                int selectedIndex = carbSwapDialog->GetSelectedIndex();
                if (selectedIndex < 0) break;
                const auto& spareCarburetors = state.GetSpareCarburetors();
                if (selectedIndex >= (int)spareCarburetors.size()) break;
                Part newCarb = spareCarburetors[selectedIndex];
                const CarPtr& car = state.GetCurrentCarPtr();
                engineBaySlotRejected[(int)EngineBaySlot::CARB0 + carbIndex] = car &&
                    (!PartsCompatibilityManager::IsVendorCompatible(newCarb.vendor, car->GetModelRef().vendor) ||
                     !PartsCompatibilityManager::IsCarburetorCompatible(newCarb.partType.carburetorType, car->GetManifoldType()));
                carbSwapDialog->Hide();
                StartCarbSlideIn(carbIndex, newCarb);
                SetActiveDialog(engineBay->GetDialog());
                break;
            }
            case 1: // Forget it
                carbSwapDialog->Hide();
                SetActiveDialog(engineBay->GetDialog());
                break;
        }
    };
    dialogConfig.texts = texts;

    carbSwapDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels, {}, [](int) {},
            listBoxRect, selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)carbSwapDialog.get());
}

std::vector<Garage::SpareRef> Garage::CollectSpares() const {
    std::vector<SpareRef> spares;
    int i = 0;
    for (const EnginePart& p : state.GetSpareEngines()) {
        std::string name = p.name.empty() ? EngineDisplayName(p) : p.name;
        spares.push_back({PartCategory::Engine, i++, name + " " + WearNote(p.wear), p.cost, p.wear});
    }
    i = 0;
    for (const Part& p : state.GetSpareTransmissions()) {
        std::string name = p.name.empty() ? TransmissionTypeName(p.partType.transmissionType, p.vendor) : p.name;
        spares.push_back({PartCategory::Transmission, i++, name + " " + WearNote(p.wear), p.cost, p.wear});
    }
    i = 0;
    for (const Part& p : state.GetSpareManifolds()) {
        std::string name = p.name.empty() ? ManifoldTypeName(p.partType.manifoldType, p.vendor) : p.name;
        spares.push_back({PartCategory::Manifold, i++, name + " " + WearNote(p.wear), p.cost, p.wear});
    }
    i = 0;
    for (const Part& p : state.GetSpareCarburetors()) {
        std::string name = p.name.empty() ? CarburetorTypeName(p.partType.carburetorType) : p.name;
        spares.push_back({PartCategory::Carburetor, i++, name + " " + WearNote(p.wear), p.cost, p.wear});
    }
    i = 0;
    for (const Part& p : state.GetSpareTires()) {
        std::string name = p.name.empty() ? WheelTypeName(p.partType.tireType) : p.name;
        spares.push_back({PartCategory::Tire, i++, name + " " + WearNote(p.wear), p.cost, p.wear});
    }
    return spares;
}

void Garage::ShowSparePartsDialog() {
    std::vector<std::string> labels;
    for (const SpareRef& s : CollectSpares()) labels.push_back(s.label);

    if (labels.empty()) return;

    SDL_Rect bounds = {36, 26, 260, 136};

    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int buttonY = bounds.y + bounds.h - buttonHeight - 8;

    SDL_Rect listBoxRect = {40, 40, 242, 80};
    SDL_Rect topRect = {37, 27, 246, 95};
    SDL_Rect bottomRect = {37, 123, 246, 40};
    SDL_Rect selectedItemRect = {39, 127, 242, 12};
    // Clicking a row only highlights it; a button acts on the highlight.
    SDL_Rect sellItRect = {bounds.x + 52, buttonY, buttonWidth, buttonHeight};
    SDL_Rect closeRect = {bounds.x + 142, buttonY, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone sellItZone("spare_parts_sell_it",    "Sell it",     sellItRect);
    Zone closeZone("spare_parts_close",       "Close",       closeRect);
    Zone scrollTopZone("spare_parts_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("spare_parts_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("spare_parts_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("spare_parts_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {sellItRect, "Sell it", nullptr, true, 0},
        {closeRect, "Close", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Spare parts", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {40, 39, 240, 1} };

    std::vector<Zone> dialogZones = {
        sellItZone, closeZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        if (buttonIndex == 0) { // Sell it
            SellSparePart(sparePartsDialog->GetSelectedIndex());
            return;
        }
        sparePartsDialog->Hide();
        SetActiveDialog(nullptr);
    };
    dialogConfig.texts = texts;

    sparePartsDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            {},
            // Enter on a highlighted row sells it.
            [this](int row) { SellSparePart(row); },
            listBoxRect,
            selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)sparePartsDialog.get());
}

void Garage::ShowTireDialog() {
    const auto& spareTires = state.GetSpareTires();
    if (spareTires.empty()) return;

    std::vector<std::string> labels;
    for (const Part& p : spareTires) {
        // The ad name it was bought under, then its wear.
        const std::string name = p.name.empty() ? WheelTypeName(p.partType.tireType) : p.name;
        labels.push_back(name + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {36, 26, 260, 136};

    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int buttonY = bounds.y + bounds.h - buttonHeight - 8;

    SDL_Rect listBoxRect = {40, 40, 242, 80};
    SDL_Rect topRect = {37, 27, 246, 95};
    SDL_Rect bottomRect = {37, 123, 246, 40};
    SDL_Rect selectedItemRect = {39, 127, 242, 12};
    SDL_Rect switchItRect = {bounds.x + 14, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {bounds.x + 165, buttonY, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone switchItZone("tire_switch_it",     "Switch it", switchItRect);
    Zone forgetItZone("tire_forget_it",     "Forget it", forgetItRect);
    Zone scrollTopZone("tire_scroll_top",   std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone("tire_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone("tire_scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("tire_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {switchItRect, "Switch it", nullptr, true,  0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Spare tires", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    std::vector<SDL_Rect> lines = { {40, 39, 240, 1} };

    std::vector<Zone> dialogZones = {
        switchItZone, forgetItZone,
        scrollTopZone, scrollDownZone, scrollUpZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = {topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Switch it
                int selectedIndex = tireDialog->GetSelectedIndex();
                if (selectedIndex < 0) break;
                const auto& spareTires = state.GetSpareTires();
                if (selectedIndex >= (int)spareTires.size()) break;
                Part newTire = spareTires[selectedIndex];
                // By row, not type: by type it could match the set just added and keep the worn one.
                state.RemoveSpareAt(PartCategory::Tire, static_cast<size_t>(selectedIndex));
                // The set coming off goes back in the parts box.
                state.AddSpareTire(state.GetCurrentCar()->GetTires());
                tireDialog->Hide();
                SetActiveDialog(nullptr);
                StartTireChangeAnimation(newTire);
                break;
            }
            case 1: // Forget it
                tireDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    dialogConfig.texts = texts;

    tireDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            {},
            [](int) {},
            listBoxRect,
            selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)tireDialog.get());
}
