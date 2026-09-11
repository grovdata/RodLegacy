#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "assets.h"
#include "parsers/libp.h"
#include <algorithm>
#include <map>

// Popup bounds: ENGINE_TRANSMISSION_WINDOW is a 200x175 window graphic (see
// docs/garage/transmission_install.md), same bounds garage_modifications.cpp's
// spare-transmissions list dialog uses.
static const SDL_Rect kTransWindowBounds = {32, 10, 200, 175};

// The window art's actual "porthole" - the transmission bitmap must stay
// clipped to this area (e.g. while sliding in from below) rather than
// spilling over the button strip or the window's border art. Clipping
// itself is ClipDialogBitmapToRect (dialogs/dialog.h) - shared with
// EngineBay, which needs the same treatment for its own part bitmaps.
// Not applied to the screws: they're drawn at 2x via AnimatedBitmap's own
// frame selection (which ClipDialogBitmapToRect's sourceRect-crop draw path
// would bypass), and both screw positions already sit inside the mask.
static const SDL_Rect kTransViewportMask = {36, 14, 192, 135};

// gear_data word 2 in the original 14-byte transmission part record was
// originally read as this bitmap's lib.p index (283-285 lined up with
// 3-speed/auto/4-speed exactly); asset 286, tentatively "racing" by that
// reading, turned out on screen to be the mounting screw, not a 4th body -
// see docs/garage/transmission_install.md. That leaves racing without a
// documented index; by elimination it's ENGINE_TRANSMISSION1 (282), the
// only one of the 4 body bitmaps otherwise unused. Unconfirmed - flag if a
// transmission type shows the wrong body art.
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

// The original doesn't table these per asset: FUN_0023236a right-aligns the
// body at x 207 and bottom-aligns it at y 130, so the per-asset padding
// falls out of the bitmap's own size. It reproduces all four hand-measured
// positions exactly - see docs/garage/transmission_install.md.
static const int kTransRightEdge = 207;
static const int kTransBottomEdge = 130;
// A part that doesn't fit is shown 24px further left before it bounces back
// out (the same call with its flag set: x 183 instead of 207).
static const int kTransRejectShift = 24;

static SDL_Point TransBitmapDestPos(const SDL_Point& natural, bool rejected = false) {
    return {kTransRightEdge - (rejected ? kTransRejectShift : 0) - natural.x,
            kTransBottomEdge - natural.y};
}

// Screw mount: the head's right edge is pinned at x 203 and the two rows
// come off the original's own table at A4-0x4442 (78 and 96, +10 for the
// blit) - matching the positions measured by hand off the running game.
static const int kScrewRightEdge = 203;

static SDL_Point ScrewPos(int index) {
    return {kScrewRightEdge, index == 0 ? 88 : 106};
}

// ENGINE_TRANSMISSION_BOLT (286, see TransmissionBitmapAsset's comment) is
// the mounting screw for this window, already drawn facing right - no
// rotation needed (unlike engine_bay.cpp's generic ENGINE_BOLT). It's a
// single image, not a sprite sheet: the screw "backing out" is 3 top-left-
// anchored crops of increasing size on the same bitmap, not distinct frames.
// Widths straight off the original's state table at A4-0x443a: 4 seated,
// 8 half out, 12 fully out, all 7 rows tall (the asset itself is 16x7, so
// the last 4 columns are never drawn). The crop is taken from the left and
// pinned by its right edge, so the screw grows out sideways rather than
// revealing more of a fixed corner.
static constexpr int kScrewCropH = 7;
static constexpr int kScrewStateW[3] = {4, 8, 12};
static constexpr float kScrewFallDuration = 0.35f;

static SDL_Rect ScrewCropForState(int state) {
    int w = kScrewStateW[state < 0 ? 0 : (state > 2 ? 2 : state)];
    return {0, 0, w, kScrewCropH};
}

// Cached on first use per asset, never re-queried after: the slide-in
// intentionally starts below the porthole (fully clipped, {0,0,0,0}), and
// whenever a DialogBitmap's clip collapses like that, Dialog::Draw()'s
// no-crop fallback path calls SetPosition/SetDimensions on the underlying
// (shared, cached-by-AssetManager) Bitmap, stomping its own rect to that
// same 0x0 - see the identical screw bug this was split out from. The first
// read for any given asset happens here, before that asset is ever drawn,
// so it's safe to trust once and freeze.
static SDL_Point TransBitmapNaturalSize(AssetManager& assets, AmigaLibAsset asset) {
    static std::map<AmigaLibAsset, SDL_Point> cache;
    auto it = cache.find(asset);
    if (it != cache.end()) return it->second;
    SDL_Rect r = assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset})->GetPosition();
    SDL_Point size = {r.w, r.h};
    cache.emplace(asset, size);
    return size;
}

// The transmission bitmap's full-size, unclipped rect at its per-asset
// mount position (TransBitmapDestPos). Callers that need to animate through
// other positions (the slide-in) use this for the natural size and the
// "arrived" endpoint, then derive their own logical rects per frame -
// clipping (ClipDialogBitmapToRect) is applied separately, never baked in here.
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

// The one place crop, push-out X, and porthole clip combine. `y` is
// whatever the caller wants right now - the resting position, or a frame of
// the fall animation.
//
// X pushes out by the same distance the crop has grown past the seated
// (state 0) width, so the screw doesn't just reveal more from a fixed
// corner - it visibly advances as it backs out. Clipped to the window's
// porthole (ClipDialogBitmapToRect, dialogs/dialog.h) same as the
// transmission bitmap; that clip is also what makes the screw disappear
// mid-fall once it passes the porthole edge rather than only when the fall
// animation itself finishes.
void Garage::ApplyScrewGeometry(int screwIndex, int y) {
    if (!transScrewBitmap[screwIndex] || !transmissionInstallDialog) return;
    DialogBitmap* db = transmissionInstallDialog->GetBitmap("screw_" + std::to_string(screwIndex));
    if (!db) return;

    SDL_Rect crop = ScrewCropForState(transScrewState[screwIndex]);
    // Right edge pinned: the screw's head stays put and its shank grows out
    // to the left as it is backed off.
    SDL_Rect logical = {ScrewPos(screwIndex).x - crop.w, y, crop.w, crop.h};
    ClipDialogBitmapToRect(*db, kTransViewportMask, logical);
}

// Sets the screw's crop/size for its current click-state, at its resting
// (non-falling) Y position.
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
            // The gearbox really leaves the car and lands in spares the
            // moment it is out, same as the engine bay's parts - otherwise
            // SELECT says you own nothing while it sits on the floor.
            Car* car = state.GetCurrentCar();
            if (car && car->HasGearbox()) {
                state.AddSpareTransmission(car->GetGearbox());
                car->RemoveGearbox();
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
    // EMPTY/SLIDING_IN: nothing to click on, ignore.
}

// Starts the screw's cosmetic drop. `fallingOut`: drops from its resting Y
// down past the window's bottom edge, then hides (screw fell out of the
// window). Otherwise: starts above the window's top edge and drops down to
// its resting Y (screw dropping in from the top for the new transmission).
//
// `delay` > 0 defers the actual start (used to stagger the two new screws
// dropping in one after the other instead of together): the screw stays
// hidden and UpdateScrewFallAnimations counts the delay down, then calls
// back into this same function with delay=0 once it elapses.
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
    // The old gearbox went to spares when it came out (OnTransScrewClicked),
    // so this only installs the new one.
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

    // db->rect holds the last frame's *clipped* position, not the logical
    // one - always re-derive the logical (unclipped) x/y from the timer
    // rather than reading it back, then clip fresh each frame.
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
            // Wrong make. The original shows it in the mount - 24px short of
            // where a fitting one sits - then takes it straight back out
            // rather than refusing the pick up front. No screws, no commit.
            transRejected = false;
            logical = {transSlideEnd.x - kTransRejectShift, transSlideEnd.y,
                       transNaturalSize.x, transNaturalSize.y};
            ClipDialogBitmapToRect(*db, kTransViewportMask, logical);
            db->bitmap->SetVisible(false);
            transInstallPhase = TransInstallPhase::EMPTY;
            bubbles[12]->SetVisible(true);  // partIncompatibleBubble
            return;
        }

        transInstallPhase = TransInstallPhase::SCREWING_IN;
        transScrewState[0] = 2;
        transScrewState[1] = 2;
        // Both StartScrewFall branches (immediate and delayed) fully own
        // their screw's visibility/position - no separate UpdateTransScrewVisual needed.
        StartScrewFall(1, /*fallingOut=*/false);
        StartScrewFall(0, /*fallingOut=*/false, /*delay=*/kScrewFallDuration);
    }
}

// Also the "changed my mind" path: backing out of a swap puts the car's own
// gearbox back. A car that dropped its transmission in a race has none to
// put back, so it comes up empty and has to be given a new one.
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

    // SELECT/DONE are drawn as part of ENGINE_TRANSMISSION_WINDOW itself, so
    // these are click-only hitboxes, not drawn buttons (absolute screen
    // coords, eyeballed against the running game).
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
    // ENGINE_TRANSMISSION_WINDOW has transparent regions (same asset the
    // spare-parts list dialog uses); without an opaque fill behind it those
    // regions show the garage scene through instead of solid black.
    cfg.contentAreas = {kTransWindowBounds};
    cfg.buttons = buttons;
    cfg.zones = {screwZone0, screwZone1};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.bitmaps["background"] = {
        state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, ENGINE_TRANSMISSION_WINDOW}),
        kTransWindowBounds, 0
    };
    cfg.bitmaps["transmission"] = BuildTransmissionBitmapEntry(car->GetGearbox().partType.transmissionType);

    // Independent copies (not a shared pointer) so hiding one screw via
    // SetVisible can't affect the other - see UpdateTransScrewVisual.
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
                // An unscrewed gearbox is already out and in spares, so Done
                // just leaves - it does not put it back. Half-unscrewed is
                // still fitted, so that only needs the screws re-seated.
                if (transInstallPhase == TransInstallPhase::UNSCREWING) {
                    ResetTransmissionInstallToMounted();
                } else if (transInstallPhase == TransInstallPhase::MOUNTED ||
                           transInstallPhase == TransInstallPhase::EMPTY) {
                    transmissionInstallDialog->Hide();
                    SetActiveDialog(nullptr);
                }
                // Mid slide/screw-in: no backing out, ignore Done.
                break;
        }
    };

    transmissionInstallDialog = std::make_unique<Dialog>(cfg);

    // SetActiveDialog first: Dialog::Show() turns every bitmap in the dialog
    // back on, so anything this car hasn't got has to be hidden AFTER it. Doing
    // it the other way round put the transmission back on screen for a car that
    // dropped its box in a race.
    SetActiveDialog(transmissionInstallDialog.get());

    // Derive the opening state from the car rather than assuming a mounted
    // gearbox: one dropped in a race leaves the window empty and waiting for
    // a replacement.
    ResetTransmissionInstallToMounted();
}
