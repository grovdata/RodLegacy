#include "garage/garage.h"
#include "core/state.h"
#include "ui/hud.h"
#include "core/assets.h"
#include "parsers/libp.h"
#include <algorithm>
#include <map>

static const SDL_Rect kTransWindowBounds = {32, 10, 200, 175};

// The porthole the transmission is clipped to. Not applied to the screws, whose frame selection the
// sourceRect path would bypass; they already sit inside it.
static const SDL_Rect kTransViewportMask = {36, 14, 192, 135};

// Racing has no documented body asset; by elimination it is ENGINE_TRANSMISSION1 (282). Unconfirmed.
static AmigaLibAsset TransmissionBitmapAsset(TransmissionType type) {
    switch (type) {
        case TRANS_AUTO_GM:
        case TRANS_AUTO_FORD:
        case TRANS_AUTO_CHRYSLER:
            return ENGINE_TRANSMISSION3;
        case TRANS_3SPD_GM:
        case TRANS_3SPD_FORD:
        case TRANS_3SPD_CHRYSLER:
            return ENGINE_TRANSMISSION2;
        case TRANS_4SPD_GM:
        case TRANS_4SPD_FORD:
        case TRANS_4SPD_CHRYSLER:
        case TRANS_4SPD_UNIVERSAL:
            return ENGINE_TRANSMISSION4;
        case TRANS_RACING_GM:
        case TRANS_RACING_FORD:
        case TRANS_RACING_CHRYSLER:
            return ENGINE_TRANSMISSION1;
    }
    return ENGINE_TRANSMISSION3;
}

// The original right-aligns the body at x 207 and bottom-aligns it at y 130.
static const int kTransRightEdge = 207;
static const int kTransBottomEdge = 130;
// A part that doesn't fit is shown 24px further left before it bounces back out.
static const int kTransRejectShift = 24;

static SDL_Point TransBitmapDestPos(const SDL_Point& natural, bool rejected = false) {
    return {kTransRightEdge - (rejected ? kTransRejectShift : 0) - natural.x,
            kTransBottomEdge - natural.y};
}

// The head's right edge is pinned at x 203; rows 78 and 96, +10 for the blit.
static const int kScrewRightEdge = 203;

static SDL_Point ScrewPos(int index) {
    return {kScrewRightEdge, index == 0 ? 88 : 106};
}

// One image, not a sheet: backing out is a left crop of width 4 / 8 / 12, 7 rows tall, pinned by the right edge.
static constexpr int kScrewCropH = 7;
static constexpr int kScrewStateW[3] = {4, 8, 12};
static constexpr float kScrewFallDuration = 0.35f;

static SDL_Rect ScrewCropForState(int state) {
    int w = kScrewStateW[state < 0 ? 0 : (state > 2 ? 2 : state)];
    return {0, 0, w, kScrewCropH};
}

// Cached on first use: Dialog::Draw's no-crop path stomps the shared bitmap's rect once a clip collapses.
static SDL_Point TransBitmapNaturalSize(AssetManager& assets, AmigaLibAsset asset) {
    static std::map<AmigaLibAsset, SDL_Point> cache;
    auto it = cache.find(asset);
    if (it != cache.end()) return it->second;
    SDL_Rect r = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset})->GetPosition();
    SDL_Point size = {r.w, r.h};
    cache.emplace(asset, size);
    return size;
}

// Unclipped mount rect; callers clip separately.
static SDL_Rect TransMountRect(AssetManager& assets, AmigaLibAsset asset) {
    SDL_Point natural = TransBitmapNaturalSize(assets, asset);
    SDL_Point pos = TransBitmapDestPos(natural);
    return {pos.x, pos.y, natural.x, natural.y};
}

DialogBitmap Garage::BuildTransmissionBitmapEntry(TransmissionType type) {
    AmigaLibAsset asset = TransmissionBitmapAsset(type);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    SDL_Rect mountRect = TransMountRect(state.GetAssets(), asset);
    DialogBitmap entry{bmp, mountRect, 1};
    ClipDialogBitmapToRect(entry, kTransViewportMask, mountRect);
    return entry;
}

// X pushes out as the crop grows, so the screw visibly advances. The porthole clip hides it mid-fall.
void Garage::ApplyScrewGeometry(int screwIndex, int y) {
    if (!transScrewBitmap[screwIndex] || !transmissionInstallDialog) return;
    DialogBitmap* db = transmissionInstallDialog->GetBitmap("screw_" + std::to_string(screwIndex));
    if (!db) return;

    SDL_Rect crop = ScrewCropForState(transScrewState[screwIndex]);
    // Right edge pinned: the shank grows out to the left.
    SDL_Rect logical = {ScrewPos(screwIndex).x - crop.w, y, crop.w, crop.h};
    ClipDialogBitmapToRect(*db, kTransViewportMask, logical);
}

void Garage::UpdateTransScrewVisual(int screwIndex) {
    ApplyScrewGeometry(screwIndex, ScrewPos(screwIndex).y);
}

void Garage::OnTransScrewClicked(int screwIndex) {
    if (transInstallPhase == TransInstallPhase::MOUNTED) {
        transInstallPhase = TransInstallPhase::UNSCREWING;
    }

    if (transInstallPhase == TransInstallPhase::UNSCREWING) {
        if (transScrewState[screwIndex] < 2) transScrewState[screwIndex]++;
        UpdateTransScrewVisual(screwIndex);
        if (transScrewState[screwIndex] == 2) {
            StartScrewFall(screwIndex, /*fallingOut=*/true);
        }
        if (transScrewState[0] == 2 && transScrewState[1] == 2) {
            if (transmissionInstallDialog) {
                DialogBitmap* db = transmissionInstallDialog->GetBitmap("transmission");
                if (db && db->bitmap) db->bitmap->SetVisible(false);
            }
            // The gearbox leaves for spares the moment it is out.
            Car* car = state.GetCurrentCar();
            if (car && car->HasGearbox()) {
                state.AddSpareTransmission(car->GetGearbox());
                car->RemoveGearbox();
                state.AdvanceTicks(900);   // lhood.c 0x2561a6
            }
            transInstallPhase = TransInstallPhase::EMPTY;
        }
    } else if (transInstallPhase == TransInstallPhase::SCREWING_IN) {
        if (transScrewState[screwIndex] > 0) transScrewState[screwIndex]--;
        UpdateTransScrewVisual(screwIndex);
        if (transScrewState[0] == 0 && transScrewState[1] == 0) {
            CommitTransmissionSwap();
            transInstallPhase = TransInstallPhase::MOUNTED;
        }
    }
}

// fallingOut drops it past the bottom and hides it; otherwise it drops in from above. delay staggers the two.
void Garage::StartScrewFall(int screwIndex, bool fallingOut, float delay) {
    if (!transmissionInstallDialog || !transScrewBitmap[screwIndex]) return;

    if (delay > 0.0f) {
        transScrewFalling[screwIndex] = false;
        transScrewFallingOut[screwIndex] = fallingOut;
        transScrewFallDelay[screwIndex] = delay;
        if (!fallingOut) transScrewBitmap[screwIndex]->SetVisible(false);
        return;
    }
    transScrewFallDelay[screwIndex] = 0.0f;

    SDL_Point pos = ScrewPos(screwIndex);
    int cropH = ScrewCropForState(transScrewState[screwIndex]).h;

    transScrewFalling[screwIndex] = true;
    transScrewFallingOut[screwIndex] = fallingOut;
    transScrewFallTimer[screwIndex] = 0.0f;
    if (fallingOut) {
        transScrewFallFromY[screwIndex] = pos.y;
        transScrewFallToY[screwIndex] = kTransWindowBounds.y + kTransWindowBounds.h + cropH;
    } else {
        transScrewFallFromY[screwIndex] = kTransWindowBounds.y - cropH;
        transScrewFallToY[screwIndex] = pos.y;
        transScrewBitmap[screwIndex]->SetVisible(true);
        ApplyScrewGeometry(screwIndex, transScrewFallFromY[screwIndex]);
    }
}

void Garage::UpdateScrewFallAnimations(double deltaTime) {
    if (!transmissionInstallDialog) return;

    for (int i = 0; i < 2; i++) {
        if (!transScrewFalling[i]) {
            if (transScrewFallDelay[i] > 0.0f) {
                transScrewFallDelay[i] -= (float)deltaTime;
                if (transScrewFallDelay[i] <= 0.0f) {
                    StartScrewFall(i, transScrewFallingOut[i], 0.0f);
                }
            }
            continue;
        }

        transScrewFallTimer[i] += (float)deltaTime;
        float t = transScrewFallTimer[i] / kScrewFallDuration;
        if (t > 1.0f) t = 1.0f;
        int y = (int)(transScrewFallFromY[i] + (transScrewFallToY[i] - transScrewFallFromY[i]) * t);
        ApplyScrewGeometry(i, y);

        if (t >= 1.0f) {
            transScrewFalling[i] = false;
            if (transScrewFallingOut[i]) {
                transScrewBitmap[i]->SetVisible(false);
            }
        }
    }
}

void Garage::CommitTransmissionSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    // The old gearbox went to spares when it came out.
    state.RemoveSpareTransmission(pendingGearbox.partType.transmissionType);
    car->SetGearbox(pendingGearbox);
}

void Garage::StartTransmissionSlideIn(const Part& newGearbox) {
    pendingGearbox = newGearbox;

    AmigaLibAsset asset = TransmissionBitmapAsset(newGearbox.partType.transmissionType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    SDL_Rect mountRect = TransMountRect(state.GetAssets(), asset);  // unclipped - the animation's logical endpoint
    transNaturalSize = {mountRect.w, mountRect.h};
    transSlideEnd = {mountRect.x, mountRect.y};
    transSlideMid = {mountRect.x - 16, mountRect.y};
    transSlideStart = {mountRect.x - 16, kTransWindowBounds.y + kTransWindowBounds.h};

    bmp->SetVisible(true);
    if (transmissionInstallDialog) {
        DialogBitmap* db = transmissionInstallDialog->GetBitmap("transmission");
        if (db) {
            db->bitmap = bmp;
            db->zOrder = 1;
            SDL_Rect logical = {transSlideStart.x, transSlideStart.y, transNaturalSize.x, transNaturalSize.y};
            ClipDialogBitmapToRect(*db, kTransViewportMask, logical);
        }
    }

    transInstallPhase = TransInstallPhase::SLIDING_IN;
    transInstallTimer = 0.0f;
}

void Garage::UpdateTransmissionInstallAnimation(double deltaTime) {
    UpdateScrewFallAnimations(deltaTime);  // runs regardless of phase

    if (transInstallPhase != TransInstallPhase::SLIDING_IN) return;
    if (!transmissionInstallDialog) return;
    DialogBitmap* db = transmissionInstallDialog->GetBitmap("transmission");
    if (!db) return;

    constexpr float SLIDE_UP_DUR = 1.1f;
    constexpr float SLIDE_RIGHT_DUR = 0.4f;

    transInstallTimer += (float)deltaTime;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto clamp01 = [](float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); };

    // db->rect holds last frame's clipped position, so re-derive from the timer.
    int logicalX;
    int logicalY;
    if (transInstallTimer < SLIDE_UP_DUR) {
        float t = clamp01(transInstallTimer / SLIDE_UP_DUR);
        logicalX = transSlideStart.x;
        logicalY = (int)lerp((float)transSlideStart.y, (float)transSlideMid.y, t);
    } else {
        float t = clamp01((transInstallTimer - SLIDE_UP_DUR) / SLIDE_RIGHT_DUR);
        logicalY = transSlideMid.y;
        logicalX = (int)lerp((float)transSlideMid.x, (float)transSlideEnd.x, t);
    }

    SDL_Rect logical = {logicalX, logicalY, transNaturalSize.x, transNaturalSize.y};
    ClipDialogBitmapToRect(*db, kTransViewportMask, logical);

    if (transInstallTimer >= SLIDE_UP_DUR + SLIDE_RIGHT_DUR) {
        logical = {transSlideEnd.x, transSlideEnd.y, transNaturalSize.x, transNaturalSize.y};
        ClipDialogBitmapToRect(*db, kTransViewportMask, logical);

        if (transRejected) {
            // Wrong make: shown 24px short of the mount, then taken straight back out.
            transRejected = false;
            logical = {transSlideEnd.x - kTransRejectShift, transSlideEnd.y,
                       transNaturalSize.x, transNaturalSize.y};
            ClipDialogBitmapToRect(*db, kTransViewportMask, logical);
            db->bitmap->SetVisible(false);
            transInstallPhase = TransInstallPhase::EMPTY;
            bubbles[12]->SetVisible(true);  // partIncompatibleBubble
            state.AdvanceTicks(1200);   // lhood.c 0x2560ec
            return;
        }

        transInstallPhase = TransInstallPhase::SCREWING_IN;
        state.AdvanceTicks(900);   // lhood.c 0x256070
        transScrewState[0] = 2;
        transScrewState[1] = 2;
        // StartScrewFall owns the screws' visibility and position.
        StartScrewFall(1, /*fallingOut=*/false);
        StartScrewFall(0, /*fallingOut=*/false, /*delay=*/kScrewFallDuration);
    }
}

// Also the "changed my mind" path. A car that dropped its transmission comes up empty.
void Garage::ResetTransmissionInstallToMounted() {
    Car* car = state.GetCurrentCar();
    if (!car || !transmissionInstallDialog) return;

    const bool fitted = car->HasGearbox();
    DialogBitmap* db = transmissionInstallDialog->GetBitmap("transmission");
    if (db) {
        *db = BuildTransmissionBitmapEntry(car->GetGearbox().partType.transmissionType);
        db->bitmap->SetVisible(fitted);
    }
    transScrewState[0] = fitted ? 0 : 2;
    transScrewState[1] = fitted ? 0 : 2;
    transScrewFalling[0] = false;
    transScrewFalling[1] = false;
    transScrewFallDelay[0] = 0.0f;
    transScrewFallDelay[1] = 0.0f;
    transInstallPhase = fitted ? TransInstallPhase::MOUNTED : TransInstallPhase::EMPTY;
    if (transScrewBitmap[0]) transScrewBitmap[0]->SetVisible(fitted);
    if (transScrewBitmap[1]) transScrewBitmap[1]->SetVisible(fitted);
    if (fitted) {
        UpdateTransScrewVisual(0);
        UpdateTransScrewVisual(1);
    }
}

void Garage::ShowTransmissionInstallDialog() {
    Car* car = state.GetCurrentCar();
    if (!car) return;

    // Drawn as part of the window art; click-only hitboxes.
    SDL_Rect selectRect = {43, 153, 61, 25};
    SDL_Rect doneRect = {161, 152, 61, 25};

    SDL_Rect screwZoneRect[2];
    for (int i = 0; i < 2; i++) {
        SDL_Point p = ScrewPos(i);
        screwZoneRect[i] = {p.x - 4, p.y - 4, 24, 16};
    }

    Zone screwZone0("trans_screw_0", "Screw", screwZoneRect[0]);
    Zone screwZone1("trans_screw_1", "Screw", screwZoneRect[1]);

    std::vector<ButtonConfig> buttons = {
        {.bounds = selectRect, .text = "Select", .onClick = nullptr,
         .defaultChoice = true, .index = 0, .invisible = true},
        {.bounds = doneRect, .text = "Done", .onClick = nullptr,
         .defaultChoice = false, .index = 1, .invisible = true, .cancelChoice = true},
    };

    DialogConfig cfg;
    cfg.bounds = kTransWindowBounds;
    cfg.backgroundColor = {0, 0, 0, 255};
    cfg.borderColor = {0, 0, 0, 0};
    // The window has transparent regions.
    cfg.contentAreas = {kTransWindowBounds};
    cfg.buttons = buttons;
    cfg.zones = {screwZone0, screwZone1};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_TRANSMISSION_WINDOW}),
        kTransWindowBounds, 0
    };
    cfg.bitmaps["transmission"] = BuildTransmissionBitmapEntry(car->GetGearbox().partType.transmissionType);

    // Independent copies, so hiding one screw can't affect the other.
    Bitmap* boltSrc = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_TRANSMISSION_BOLT});
    for (int i = 0; i < 2; i++) {
        transScrewBitmap[i] = std::make_unique<Bitmap>(*boltSrc);
        transScrewBitmap[i]->SetVisible(true);
        SDL_Rect natural = transScrewBitmap[i]->GetPosition();
        transScrewNaturalSize[i] = {natural.w, natural.h};
        SDL_Rect crop = ScrewCropForState(0);
        SDL_Point p = ScrewPos(i);
        cfg.bitmaps["screw_" + std::to_string(i)] = {
            transScrewBitmap[i].get(), {p.x - crop.w, p.y, crop.w, crop.h}, 2, crop
        };
    }

    cfg.onZoneHover = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onZoneClick = [this](Zone& zone) {
        if (zone.GetId() == "trans_screw_0") OnTransScrewClicked(0);
        else if (zone.GetId() == "trans_screw_1") OnTransScrewClicked(1);
    };
    cfg.onButtonPressed = [this](int index) {
        switch (index) {
            case 0:  // Select
                ShowTransmissionDialog();
                break;
            case 1:  // Done
                // An unscrewed gearbox is already in spares, so Done just leaves; half-unscrewed only needs re-seating.
                if (transInstallPhase == TransInstallPhase::UNSCREWING) {
                    ResetTransmissionInstallToMounted();
                } else if (transInstallPhase == TransInstallPhase::MOUNTED ||
                           transInstallPhase == TransInstallPhase::EMPTY) {
                    transmissionInstallDialog->Hide();
                    SetActiveDialog(nullptr);
                }
                break;
        }
    };

    transmissionInstallDialog = std::make_unique<Dialog>(cfg);

    // First: Show() turns every bitmap on, so whatever this car lacks is hidden after.
    SetActiveDialog(transmissionInstallDialog.get());

    ResetTransmissionInstallToMounted();
}
