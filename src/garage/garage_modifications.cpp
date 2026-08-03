#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"
#include "embedded_loader.h"
#include "audio/audio_manager.h"
#include "engine_part.h"
#include "parsers/libp.h"
#include <SDL_image.h>
#include <algorithm>

void Garage::ShowBumperDialog(bool isFront) {
    // Hide any previous error bubble
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }

    // Check if car can strip bumpers
    if (state.GetCurrentCar()) {
        CarModel model = state.GetCurrentCar()->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;

        // If trying to strip bumpers, check if car supports it
        bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
        if (!isStripped && !CarQuery::SrLibQueryCanStripBumpers(bodyAssetNo)) {
            // Show constraint bubble instead of dialog
            if (bubbles.size() > 6) {
                bubbles[6]->SetVisible(true);  // bumperConstraintBubble
            }
            return;
        }
    }

    // Check current bumper status
    bool isStripped = isFront ? carBitmap->IsFrontBumperStripped() : carBitmap->IsRearBumperStripped();
    const char* titleText = isFront ? (isStripped ? "Restore bumper - $20" : "Strip bumper - $20")
                                    : (isStripped ? "Restore bumper - $20" : "Strip bumper - $20");

    std::unique_ptr<Dialog>& dialogPtr = isFront ? frontBumperDialog : rearBumperDialog;

    auto onConfirm = [this, isFront, isStripped, &dialogPtr]() {
        if (carBitmap) {
            // Play sound before the operation so it isn't delayed by texture loading
            if (audioManager) {
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
            }
            if (isStripped) {
                // Restore
                if (isFront) {
                    carBitmap->RestoreFrontBumper();
                    LOG_INFO("Front bumper restored for $20");
                } else {
                    carBitmap->RestoreRearBumper();
                    LOG_INFO("Rear bumper restored for $20");
                }
            } else {
                // Strip
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
    // Hide any previous error bubble
    if (bubbles.size() > 4) {
        bubbles[4]->SetVisible(false);
    }

    // Check if car can chop roof
    if (state.GetCurrentCar()) {
        CarModel model = state.GetCurrentCar()->GetModel();
        AmigaLibAsset bodyAssetNo = (AmigaLibAsset)model.index;

        // If trying to chop roof, check if car supports it
        bool isChopped = carBitmap->IsRoofChopped();
        if (!isChopped && !CarQuery::SrLibQueryCanChopRoof(bodyAssetNo)) {
            // Show constraint bubble instead of dialog
            if (bubbles.size() > 5) {
                bubbles[5]->SetVisible(true);  // roofConstraintBubble
            }
            return;
        }
    }

    // Check current roof status
    bool isChopped = carBitmap->IsRoofChopped();
    const char* titleText = isChopped ? "Restore roof - $70" : "Chop roof - $70";

    auto onConfirm = [this, isChopped]() {
        if (carBitmap) {
            // Play sound before the operation so it isn't delayed by texture loading
            if (audioManager) {
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f);
            }
            if (isChopped) {
                // Restore
                carBitmap->RestoreRoof();
                LOG_INFO("Roof restored for $70");
            } else {
                // Chop
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
    // Hide any error bubble when cycling colors
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::CyclePreviousColor() {
    int prevColor = (previewColor - 1 + CarColorCount) % CarColorCount;
    previewColor = static_cast<StandardCarColor>(prevColor);
    UpdatePaintCanPreview();
    // Hide any error bubble when cycling colors
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }
}

void Garage::ApplyPreviewColor() {
    if (carBitmap) {
        carBitmap->UpdateColor(previewColor);
        // Update paint can to show the new current color
        UpdatePaintCanPreview();
    }
}

void Garage::UpdatePaintCanPreview() {
    if (!paintCanPreview) return;

    // Load the paint can mask image from embedded resource
    extern const unsigned char embedded_paintcan_png[];
    extern const size_t embedded_paintcan_png_len;

    SDL_Surface* maskSurface = LoadSurfaceFromMemory(embedded_paintcan_png, embedded_paintcan_png_len);
    if (!maskSurface) {
        LOG_INFO("Failed to load embedded paintcan.png");
        return;
    }

    // Get the preview color palette
    std::vector<uint16_t> colorPalette = GetStandardCarColor(previewColor);

    // Convert the mask surface to 32-bit RGBA
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(maskSurface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(maskSurface);

    if (!rgbaSurface) {
        LOG_INFO("Failed to convert paintcan surface");
        return;
    }

    // Apply the car color palette to the paint can.
    // The mask PNG is a grayscale+alpha image. Transparent pixels are background.
    // Opaque pixel brightness (0-255) maps linearly to the 9-entry color palette.
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

    // Update the bitmap with the new surface
    paintCanPreview->ReplaceSurface(rgbaSurface);
}

void Garage::ShowPaintDialog() {
    // Store the original color so we can restore it if user cancels
    if (carBitmap) {
        originalColor = carBitmap->GetCurrentColor();
        previewColor = originalColor;
    }

    // Hide any previous error bubble
    if (bubbles.size() > 3) {
        bubbles[3]->SetVisible(false);
    }

    // Update paint can to show preview color
    UpdatePaintCanPreview();

    // Button positions as specified
    SDL_Rect nextColorRect = {149, 127, 78, 16};
    SDL_Rect prevColorRect = {233, 127, 78, 16};
    SDL_Rect goAheadRect = {149, 145, 78, 16};
    SDL_Rect forgetItRect = {233, 145, 78, 16};

    ButtonConfig nextColorButton = {nextColorRect, "Next color", nullptr, false, 0};
    ButtonConfig prevColorButton = {prevColorRect, "Previous color", nullptr, false, 1};
    ButtonConfig goAheadButton = {goAheadRect, "Go ahead", nullptr, true, 2}; // Default
    ButtonConfig forgetItButton = {forgetItRect, "Forget it", nullptr, false, 3};

    std::vector<ButtonConfig> buttons = {nextColorButton, prevColorButton, goAheadButton, forgetItButton};

    DialogConfig paintDialogConfig;
    paintDialogConfig.bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT}; // Full screen but transparent
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
                        Car* car = state.GetCurrentCar();
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
                        // Show "Paint ain't free!" bubble and keep dialog open
                        LOG_INFO("Not enough money! Need $20");
                        if (bubbles.size() > 3) {
                            bubbles[3]->SetVisible(true);  // paintBubble
                        }
                    }
                }
                break;
            case 3: // Forget it
                // Just close - restore original color on paint can
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

    // Create and show the dialog
    paintDialog = std::make_unique<Dialog>(paintDialogConfig);
    SetActiveDialog(paintDialog.get());
}

void Garage::ShowCarInfoDialog() {
    // Create dialog bounds
    SDL_Rect dialogRect = {37, 27, 246, 116};

    std::vector<DialogTextConfig> texts;
    // Title centered
    texts.push_back(DialogTextConfig{SDL_Rect{dialogRect.x + 3, dialogRect.y + 3, dialogRect.w - 6, 10}, "Car parts", {0xFF, 0xFF, 0xFF, 0xFF}, true});

    // Horizontal line under title
    std::vector<SDL_Rect> lines = {
        {dialogRect.x + 3, dialogRect.y + 14, dialogRect.w - 6, 1}
    };

    // Content text area start Y
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
        // Render left, then compute right-aligned note position using measured width
        texts.push_back(DialogTextConfig{SDL_Rect{xLeft, y, dialogRect.w - 16, 10}, left, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        int noteWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), note);
        texts.push_back(DialogTextConfig{SDL_Rect{xRightAlign - noteWidth, y, noteWidth, 10}, note, {0xFF, 0xFF, 0xFF, 0xFF}, false});
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
    };

    // Populate from current car parts (engine, manifold, carb, transmission, tires)
    Car* car = state.GetCurrentCar();
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
            if (wear <= 0) return std::string("(new)");
            char buf[32];
            snprintf(buf, sizeof(buf), "(%d%% worn)", wear);
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
                    // Fallback to vendor auto
                    std::string v = vendorToStr(vendor);
                    return std::string("Auto Trnsmsn ") + v + ".";
                }
            }
        };
        auto carbName = [](AmigaLibAsset aid) -> const char* {
            switch (aid) {
                case ENGINE_CARB_2BARREL: return "2-brl carb. Fits all.";
                case ENGINE_CARB_4BARREL: return "4-brl carb.";
                case ENGINE_CARB_4BARREL_RACING: return "Racing 4-brl carb.";
                default: return "carburetor.";
            }
        };

        CarVendor_t vendor = model.vendor;
        if (vendor == CarVendorUnknown) {
            vendor = SrLibQueryGetCarVendor(model.index);
        }

        // Debug: Car Vendor
        std::string vendorLabel = "Car Vendor: " + std::string(vendorToStr(vendor));
        addLeft(vendorLabel.c_str());

        // Engine line
        EnginePart engine = car->GetEngine();
        std::string engineLabel = engine.GetEngineName();
        addRightNote(engineLabel.c_str(), wearNote(engine.wear).c_str());
        // Manifold line (based on carb type if possible)
        const Part carb = car->GetCarburator();
        const char* maniText = (carb.assetId == ENGINE_CARB_2BARREL) ? "  2-brl manifold." : "  4-brl manifold.";
        addIndented(maniText);
        // Carb line
        std::string carbText = std::string("  ") + carbName(carb.assetId);
        addIndented(carbText.c_str());
        // Transmission line
        const Part trans = car->GetGearbox();
        std::string transText = transName(trans.assetId, vendor);
        addRightNote(transText.c_str(), wearNote(trans.wear).c_str());
        // Tires line
        addRightNote("Tires.", wearNote(car->GetTires().wear).c_str());

        // Blank line
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        // Fuel (placeholder until wired)
        addLeft("Fuel     4 gallons");
        // Blank line
        y += SFont_TextHeight(state.GetAssets().GetMainFont());
        int mph = model.baseSpeed > 0 ? model.baseSpeed : 97;
        char speedBuf[64];
        snprintf(speedBuf, sizeof(speedBuf), "Max speed   %d mph", mph);
        addLeft(speedBuf);
    }

    DialogConfig infoCfg;
    infoCfg.bounds = dialogRect;
    infoCfg.backgroundColor = {0, 208, 0, 255};
    infoCfg.borderColor = {0, 111, 239, 255};
    infoCfg.contentAreas = { dialogRect }; // draw dialog background
    infoCfg.texts = texts;
    infoCfg.lines = lines;
    infoCfg.font = state.GetAssets().GetMainFont();

    // No buttons - dialog is dismissed by clicking outside or pressing Escape
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
    switch (month) {
        case 6:
            return SDL_Point{114, 39};
        case 7:
        switch (day) {
            case 1:
                return SDL_Point{114, 39};
            case 2:
                return SDL_Point{114, 39};
            case 3:
                return SDL_Point{114, 39};
        }
            return SDL_Point{114, 39};
        case 8:
            return SDL_Point{114, 39};
    }
    return SDL_Point{0, 0};
}

void Garage::ShowCalendarDialog() {
    // Load the June/July calendar bitmap (hardcoded to July for now)
    Bitmap* calBitmap = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, CALENDAR_JUNE_JULY_SCREEN});
    if (!calBitmap) {
        LOG_INFO("Failed to load calendar bitmap");
        return;
    }

    int month = 7;
    int day = 16;

    int calW = calBitmap->GetSurface()->w;
    int calH = calBitmap->GetSurface()->h;

    int dialogX = 80;
    int dialogY = 4;
    SDL_Rect dialogRect = {dialogX, dialogY, calW, calH};

    std::map<std::string, DialogBitmap> bitmaps;
    bitmaps["calendar"] = {calBitmap, dialogRect, 0};

    // Load the date rectangle highlight
    Bitmap* dateRect = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, CALENDAR_CURRENT_DATE_RECTANGLE});
    if (dateRect) {
        // TODO: Position the date rectangle on the correct day cell
        // For now, place it on July 2 (first row, second column of the July grid)
        // This needs to be mapped out from the actual calendar image layout
        int dateW = dateRect->GetSurface()->w;
        int dateH = dateRect->GetSurface()->h;
        SDL_Point datePos = GetCalendarDateHighlightPosition(month, day);
        SDL_Rect dateHighlightRect = {datePos.x, datePos.y, dateW, dateH};
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
            // Case-insensitive .png check
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
    // Only reload custom stickers on first open (scrollRow == 0 means fresh open)
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

    // Clamp scroll position
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


    // Populate visible cells based on scroll position
    // Layout: items 0-7 = Amiga stock, 8-15 = DOS (if available), then custom
    for (int displayRow = 0; displayRow < VISIBLE_ROWS; displayRow++) {
        int actualRow = stickerScrollRow + displayRow;
        for (int c = 0; c < 4; c++) {
            int itemIndex = actualRow * 4 + c;
            if (itemIndex >= totalItems) break;

            SDL_Rect dest = {GRID_LEFT + c * cellW, GRID_TOP + displayRow * cellH, cellW, cellH};

            if (itemIndex < 8) {
                // Amiga stock sticker from spritesheet
                int stockRow = itemIndex / 4;
                int stockCol = itemIndex % 4;
                SDL_Rect src = {stockCol * SRC_CELL_W, stockRow * SRC_CELL_H, SRC_CELL_W, SRC_CELL_H};
                DialogBitmap db;
                db.bitmap = decalsSheet;
                db.rect = dest;
                db.zOrder = 0;
                db.sourceRect = src;
                bitmaps["decal_" + std::to_string(itemIndex)] = db;
                zones.push_back(Zone("decal_" + std::to_string(itemIndex), SrLibCarDecalNames[itemIndex], dest));
            } else if (dosDecalCount > 0 && itemIndex < 8 + dosDecalCount) {
                // DOS sticker - use native dimensions, center in cell, scale only if too big
                int dosIndex = itemIndex - 8;
                Bitmap* dosBmp = state.GetAssets().GetDosDecal(dosIndex);
                if (dosBmp) {
                    SDL_Rect native = dosBmp->GetSourceRect();
                    int drawW = native.w;
                    int drawH = native.h;
                    // Scale down proportionally if larger than cell
                    if (drawW > cellW || drawH > cellH) {
                        float scaleW = (float)cellW / drawW;
                        float scaleH = (float)cellH / drawH;
                        float scale = std::min(scaleW, scaleH);
                        drawW = (int)(drawW * scale);
                        drawH = (int)(drawH * scale);
                    }
                    // Center in cell
                    SDL_Rect centered = {
                        dest.x + (cellW - drawW) / 2,
                        dest.y + (cellH - drawH) / 2,
                        drawW, drawH
                    };
                    DialogBitmap db;
                    db.bitmap = dosBmp;
                    db.rect = centered;
                    db.zOrder = 0;
                    bitmaps["dos_" + std::to_string(dosIndex)] = db;
                    zones.push_back(Zone("dos_" + std::to_string(dosIndex), SrLibCarDecalNames[8 + dosIndex], dest));
                }
            } else {
                // Custom sticker
                int customIndex = itemIndex - 8 - dosDecalCount;
                DialogBitmap db;
                db.bitmap = customStickerBitmaps[customIndex].get();
                db.rect = dest;
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

    // Scroll buttons outside dialog, same pattern as car picker
    const int scrollX = 289;
    const int scrollBtnW = 15;
    const int scrollBtnH = 15;
    SDL_Rect scrollTopRect    = {scrollX, dialogRect.y, scrollBtnW, scrollBtnH};
    SDL_Rect scrollUpRect     = {scrollX, dialogRect.y + 16, scrollBtnW, scrollBtnH};
    SDL_Rect scrollDownRect   = {scrollX, dialogRect.y + dialogH - 31, scrollBtnW, scrollBtnH};
    SDL_Rect scrollBottomRect = {scrollX, dialogRect.y + dialogH - 15, scrollBtnW, scrollBtnH};

    zones.push_back(Zone("sticker_scroll_top",    "\\", scrollTopRect));
    zones.push_back(Zone("sticker_scroll_up",     "<",  scrollUpRect));
    zones.push_back(Zone("sticker_scroll_down",   ">",  scrollDownRect));
    zones.push_back(Zone("sticker_scroll_bottom", "/",  scrollBottomRect));

    std::vector<ButtonConfig> buttons;
    ButtonConfig scrollTopBtn = {scrollTopRect, "\\", nullptr, false, 1};
    scrollTopBtn.fillColor = frameBg;
    scrollTopBtn.borderColor = frameBorder;
    scrollTopBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollUpBtn = {scrollUpRect, "<", nullptr, false, 2};
    scrollUpBtn.fillColor = frameBg;
    scrollUpBtn.borderColor = frameBorder;
    scrollUpBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollDownBtn = {scrollDownRect, ">", nullptr, false, 3};
    scrollDownBtn.fillColor = frameBg;
    scrollDownBtn.borderColor = frameBorder;
    scrollDownBtn.textColor = ButtonDefaultChoiceColor;
    ButtonConfig scrollBottomBtn = {scrollBottomRect, "/", nullptr, false, 4};
    scrollBottomBtn.fillColor = frameBg;
    scrollBottomBtn.borderColor = frameBorder;
    scrollBottomBtn.textColor = ButtonDefaultChoiceColor;
    buttons = {scrollTopBtn, scrollUpBtn, scrollDownBtn, scrollBottomBtn};

    DialogConfig stickerCfg;
    // Bounds includes scroll buttons for dismiss hit-testing; contentAreas is the visual rect
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
            Car* car = state.GetCurrentCar();
            if (car && car->CanAddDecal()) {
                int money = state.GetBankroll();
                if (money >= 5) {
                    state.SetBankroll(money - 5);
                    state.AdvanceGameHours(1);
                    applyFn(car);
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
    // Dialog dimensions as specified
    const int dialogWidth = 156;
    const int dialogHeight = 36;
    const int dialogX = 82;
    const int dialogY = 50;

    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};

    // Check if player has enough money
    // int currentMoney = state.GetBankroll();
    // bool canAfford = currentMoney >= price;

    // Button layout
    const int buttonWidth = 70;
    const int buttonHeight = 14;
    const int buttonSpacing = 4;
    const int buttonY = dialogY + dialogHeight - buttonHeight - 4;

    SDL_Rect okButtonRect = {dialogX + 4, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetButtonRect = {dialogX + 4 + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight};

    ButtonConfig okButton = {okButtonRect, "OK", nullptr, true, 0}; // OK is default
    ButtonConfig forgetButton = {forgetButtonRect, "Forget it", nullptr, false, 1};

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
                        // Show "Customization ain't free!" bubble and keep dialog open
                        LOG_INFO("Not enough money! Need $%d", price);
                        if (bubbles.size() > 4) {
                            bubbles[4]->SetVisible(true);  // customizationBubble
                        }
                        return;
                    }

                    // Deduct price
                    state.SetBankroll(currentMoney - price);

                    // Execute the modification
                    onConfirm();

                    // Hide bubble and close dialog
                    if (bubbles.size() > 4) {
                        bubbles[4]->SetVisible(false);
                    }
                    dialogPtr->Hide();
                    SetActiveDialog(nullptr);
                }
                break;
            case 1: // Forget it
                // Hide bubble when closing
                if (bubbles.size() > 4) {
                    bubbles[4]->SetVisible(false);
                }
                dialogPtr->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };

    // Create and show the dialog
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

void Garage::ShowTransmissionDialog() {
    SDL_Rect bounds = {32, 9, 200, 175};
}

void Garage::ShowTireDialog() {
    const auto& spareTires = state.GetSpareTires();
    if (spareTires.empty()) return;

    std::vector<std::string> labels;
    for (CarWheelType t : spareTires) {
        labels.push_back(WheelTypeName(t));
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
    Zone scrollTopZone("tire_scroll_top",   "\\",        scrollTopRect);
    Zone scrollDownZone("tire_scroll_down", ">",         scrollDownRect);
    Zone scrollUpZone("tire_scroll_up",     "<",         scrollUpRect);
    Zone scrollBottomZone("tire_scroll_bottom", "/",     scrollBottomRect);

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
                CarWheelType newType = spareTires[selectedIndex];
                CarWheelType oldType = state.GetCurrentCar()->GetWheelType();
                state.AddSpareTire(oldType);
                state.RemoveSpareTire(newType);
                tireDialog->Hide();
                SetActiveDialog(nullptr);
                StartTireChangeAnimation(newType);
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
            {0xFF, 0xFF, 0xFF, 0xFF},
            {0xFF, 0xFF, 0x00, 0xFF},
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );
    SetActiveDialog((Dialog*)tireDialog.get());
}
