#include "garage.h"
#include "engine_bay_layout.h"
#include "state.h"
#include "ui/hud.h"
#include "assets.h"
#include "parsers/libp.h"
#include "car_builder.h"
#include <algorithm>

using EngineBaySlot = Garage::EngineBaySlot;
using PartInstallPhase = Garage::PartInstallPhase;

// EngineBay's own overview bounds/porthole (engine_bay.cpp's engineBayRect /
// engineBayFillRect) - reused here so slide-ins stay clipped inside the
// window's visible art, same idea as the transmission's kTransViewportMask.
static const SDL_Rect kEngineBayBounds = {24, 0, 200, 189};
static const SDL_Rect kEngineBayMask = {28, 4, 193, 181};

static constexpr float kScrewFallDuration = 0.35f;
static constexpr float kSlideUpDuration = 1.1f;
static constexpr float kSlideRightDuration = 0.4f;

// screwIndex 0=engine, 1=manifold-left, 2=manifold-right, 3=carb.
static EngineBaySlot ScrewSlot(int screwIndex) {
    if (screwIndex == 0) return EngineBaySlot::ENGINE;
    if (screwIndex == 1 || screwIndex == 2) return EngineBaySlot::MANIFOLD;
    return EngineBaySlot::CARB;
}

static int SlotFirstScrew(EngineBaySlot slot) {
    switch (slot) {
        case EngineBaySlot::ENGINE: return 0;
        case EngineBaySlot::MANIFOLD: return 1;
        case EngineBaySlot::CARB: return 3;
    }
    return 0;
}

static int SlotScrewCount(EngineBaySlot slot) {
    return slot == EngineBaySlot::MANIFOLD ? 2 : 1;
}

static const char* SlotBitmapId(EngineBaySlot slot) {
    switch (slot) {
        case EngineBaySlot::ENGINE: return "engine_body";
        case EngineBaySlot::MANIFOLD: return "manifold_body";
        case EngineBaySlot::CARB: return "carb_body";
    }
    return "engine_body";
}

// Screw mount point: x is the sprite's left edge, y its bottom edge.
static SDL_Point ScrewPos(EngineBay* bay, int screwIndex) {
    return bay ? bay->GetScrewAnchor(screwIndex) : SDL_Point{0, 0};
}

// ENGINE_BOLT is a real 3-frame sheet (one 8x4 frame per row) - state
// selects a frame, not a growing crop like the transmission's screws.
static SDL_Rect ScrewFrameCrop(const SDL_Point& natural, int state) {
    int frameH = natural.y / 3;
    return {0, state * frameH, natural.x, frameH};
}

// bottomY is the screw's bottom edge - the crop shrinks as it is driven in,
// so anchoring the top would slide it off its mount.
void Garage::ApplyEngineBayScrewGeometry(int screwIndex, int bottomY) {
    if (!engineBay) return;
    Dialog* dlg = engineBay->GetDialog();
    if (!dlg) return;
    DialogBitmap* db = dlg->GetBitmap("screw_" + std::to_string(screwIndex));
    if (!db) return;

    SDL_Point natural = engineBay->GetScrewNaturalSize(screwIndex);
    SDL_Rect crop = ScrewFrameCrop(natural, engineBayScrewState[screwIndex]);
    SDL_Point pos = ScrewPos(engineBay.get(), screwIndex);
    SDL_Rect logical = {pos.x, bottomY - crop.h, crop.w, crop.h};
    ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
}

void Garage::UpdateEngineBayScrewVisual(int screwIndex) {
    ApplyEngineBayScrewGeometry(screwIndex, ScrewPos(engineBay.get(), screwIndex).y);
}

void Garage::StartEngineBayScrewFall(int screwIndex, bool fallingOut, float delay) {
    if (!engineBay) return;
    Bitmap* bmp = engineBay->GetScrewBitmap(screwIndex);
    if (!bmp) return;

    if (delay > 0.0f) {
        engineBayScrewFalling[screwIndex] = false;
        engineBayScrewFallingOut[screwIndex] = fallingOut;
        engineBayScrewFallDelay[screwIndex] = delay;
        if (!fallingOut) bmp->SetVisible(false);
        return;
    }
    engineBayScrewFallDelay[screwIndex] = 0.0f;

    SDL_Point pos = ScrewPos(engineBay.get(), screwIndex);
    SDL_Point natural = engineBay->GetScrewNaturalSize(screwIndex);
    int cropH = ScrewFrameCrop(natural, engineBayScrewState[screwIndex]).h;

    engineBayScrewFalling[screwIndex] = true;
    engineBayScrewFallingOut[screwIndex] = fallingOut;
    engineBayScrewFallTimer[screwIndex] = 0.0f;
    if (fallingOut) {
        engineBayScrewFallFromY[screwIndex] = pos.y;
        engineBayScrewFallToY[screwIndex] = kEngineBayBounds.y + kEngineBayBounds.h + cropH;
    } else {
        engineBayScrewFallFromY[screwIndex] = kEngineBayBounds.y;
        engineBayScrewFallToY[screwIndex] = pos.y;
        bmp->SetVisible(true);
        ApplyEngineBayScrewGeometry(screwIndex, engineBayScrewFallFromY[screwIndex]);
    }
}

void Garage::UpdateEngineBayScrewFallAnimations(double deltaTime) {
    if (!engineBay) return;

    for (int i = 0; i < 4; i++) {
        if (!engineBayScrewFalling[i]) {
            if (engineBayScrewFallDelay[i] > 0.0f) {
                engineBayScrewFallDelay[i] -= (float)deltaTime;
                if (engineBayScrewFallDelay[i] <= 0.0f) {
                    StartEngineBayScrewFall(i, engineBayScrewFallingOut[i], 0.0f);
                }
            }
            continue;
        }

        engineBayScrewFallTimer[i] += (float)deltaTime;
        float t = engineBayScrewFallTimer[i] / kScrewFallDuration;
        if (t > 1.0f) t = 1.0f;
        int y = (int)(engineBayScrewFallFromY[i] + (engineBayScrewFallToY[i] - engineBayScrewFallFromY[i]) * t);
        ApplyEngineBayScrewGeometry(i, y);

        if (t >= 1.0f) {
            engineBayScrewFalling[i] = false;
            if (engineBayScrewFallingOut[i]) {
                engineBay->GetScrewBitmap(i)->SetVisible(false);
            }
        }
    }
}

// The engine slot additionally needs its ground wire unplugged/replugged -
// everyone else just needs their screws done.
bool Garage::IsEngineBaySlotFullyOut(EngineBaySlot slot) {
    int first = SlotFirstScrew(slot);
    for (int i = first; i < first + SlotScrewCount(slot); i++) {
        if (engineBayScrewState[i] != 2) return false;
    }
    if (slot == EngineBaySlot::ENGINE && engineWireAttached) return false;
    return true;
}

bool Garage::IsEngineBaySlotFullySeated(EngineBaySlot slot) {
    int first = SlotFirstScrew(slot);
    for (int i = first; i < first + SlotScrewCount(slot); i++) {
        if (engineBayScrewState[i] != 0) return false;
    }
    if (slot == EngineBaySlot::ENGINE && !engineWireAttached) return false;
    return true;
}

// Called the moment a slot goes fully EMPTY (last screw out / wire
// unplugged) so the just-removed part is immediately available again in its
// swap dialog - Commit*Swap used to be the only place a part was added to
// spares, which meant it stayed invisible to the picker until you fitted
// something else in its place.
void Garage::ReleaseEngineBayPartToSpares(EngineBaySlot slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    switch (slot) {
        case EngineBaySlot::ENGINE: {
            EnginePart old;
            if (car->GetSpecEngineClass() >= 0) {
                CarEngine_t type = ResolveEngineType(*car);
                old = EnginePart(type, 0, 0, car->GetModelRef().vendor, EngineBodyAsset(type));
            } else {
                old = car->GetEngine();
            }
            state.AddSpareEngine(old);
            break;
        }
        case EngineBaySlot::MANIFOLD: {
            Part old = car->GetSpecManifoldClass() >= 0
                ? CarBuilder::CreateManifoldPart(ResolveManifoldType(*car), car->GetModelRef().vendor)
                : car->GetManifold();
            state.AddSpareManifold(old);
            break;
        }
        case EngineBaySlot::CARB: {
            state.AddSpareCarburetor(car->GetCarburator());
            break;
        }
    }
}

void Garage::OnEngineBayScrewClicked(int screwIndex) {
    EngineBaySlot slot = ScrewSlot(screwIndex);
    int slotIdx = (int)slot;

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::MOUNTED) {
        engineBaySlotPhase[slotIdx] = PartInstallPhase::UNSCREWING;
    }

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::UNSCREWING) {
        if (engineBayScrewState[screwIndex] < 2) engineBayScrewState[screwIndex]++;
        UpdateEngineBayScrewVisual(screwIndex);
        if (engineBayScrewState[screwIndex] == 2) {
            StartEngineBayScrewFall(screwIndex, /*fallingOut=*/true);
        }

        if (IsEngineBaySlotFullyOut(slot)) {
            if (engineBay) {
                DialogBitmap* db = engineBay->GetDialog()->GetBitmap(SlotBitmapId(slot));
                if (db && db->bitmap) db->bitmap->SetVisible(false);
            }
            ReleaseEngineBayPartToSpares(slot);
            engineBaySlotPhase[slotIdx] = PartInstallPhase::EMPTY;
        }
    } else if (engineBaySlotPhase[slotIdx] == PartInstallPhase::SCREWING_IN) {
        if (engineBayScrewState[screwIndex] > 0) engineBayScrewState[screwIndex]--;
        UpdateEngineBayScrewVisual(screwIndex);

        if (IsEngineBaySlotFullySeated(slot)) {
            switch (slot) {
                case EngineBaySlot::ENGINE: CommitEngineSwap(); break;
                case EngineBaySlot::MANIFOLD: CommitManifoldSwap(); break;
                case EngineBaySlot::CARB: CommitCarbSwap(); break;
            }
            engineBaySlotPhase[slotIdx] = PartInstallPhase::MOUNTED;
        }
    }
    // EMPTY/SLIDING_IN: nothing to click on, ignore.
}

void Garage::OnEngineWireClicked() {
    EngineBaySlot slot = EngineBaySlot::ENGINE;
    int slotIdx = (int)slot;

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::MOUNTED) {
        engineBaySlotPhase[slotIdx] = PartInstallPhase::UNSCREWING;
    }
    if (engineBaySlotPhase[slotIdx] != PartInstallPhase::UNSCREWING &&
        engineBaySlotPhase[slotIdx] != PartInstallPhase::SCREWING_IN) {
        return;  // EMPTY/SLIDING_IN: nothing to click on, ignore.
    }

    engineWireAttached = !engineWireAttached;
    if (engineBay) {
        DialogBitmap* db = engineBay->GetDialog()->GetBitmap("engine_wire");
        if (db) {
            db->bitmap = state.GetAssets().GetBitmapAsset(
                {LibAssetRegular, SrLibFileAmigaLibP, engineWireAttached ? ENGINE_WIRE_ATTACHED : ENGINE_WIRE_DETACHED});
        }
    }

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::UNSCREWING && IsEngineBaySlotFullyOut(slot)) {
        if (engineBay) {
            DialogBitmap* db = engineBay->GetDialog()->GetBitmap(SlotBitmapId(slot));
            if (db && db->bitmap) db->bitmap->SetVisible(false);
        }
        ReleaseEngineBayPartToSpares(slot);
        engineBaySlotPhase[slotIdx] = PartInstallPhase::EMPTY;
    } else if (engineBaySlotPhase[slotIdx] == PartInstallPhase::SCREWING_IN && IsEngineBaySlotFullySeated(slot)) {
        CommitEngineSwap();
        engineBaySlotPhase[slotIdx] = PartInstallPhase::MOUNTED;
    }
}

void Garage::OnEngineBayPartClicked(EngineBaySlot slot) {
    if (engineBaySlotPhase[(int)slot] != PartInstallPhase::EMPTY) return;
    switch (slot) {
        case EngineBaySlot::ENGINE: ShowEngineDialog(); break;
        case EngineBaySlot::MANIFOLD: ShowManifoldDialog(); break;
        case EngineBaySlot::CARB: ShowCarburetorDialog(); break;
    }
}

// The pre-existing "Parts" button used to open a hardcoded sample list
// unrelated to the car. Now it's a second way into whichever slot is
// currently disassembled (the main way is clicking the emptied part
// itself - see OnEngineBayPartClicked/part_N zones).
void Garage::OnEngineBayPartsButtonClicked() {
    for (int i = 0; i < 3; i++) {
        if (engineBaySlotPhase[i] == PartInstallPhase::EMPTY) {
            OnEngineBayPartClicked((EngineBaySlot)i);
            return;
        }
    }
    if (hud) hud->SetText("Unscrew a part first");
}

void Garage::OnEngineBayZoneClicked(Zone& zone) {
    const std::string& id = zone.GetId();
    if (id.rfind("screw_", 0) == 0) {
        OnEngineBayScrewClicked(std::stoi(id.substr(6)));
        return;
    }
    if (id == "wire_0") { OnEngineWireClicked(); return; }
    if (id == "part_0") { OnEngineBayPartClicked(EngineBaySlot::ENGINE); return; }
    if (id == "part_1") { OnEngineBayPartClicked(EngineBaySlot::MANIFOLD); return; }
    if (id == "part_2") { OnEngineBayPartClicked(EngineBaySlot::CARB); return; }
}

static void BeginSlide(const SDL_Rect& mount, SDL_Point& start, SDL_Point& mid, SDL_Point& end) {
    // All engine bay parts drop straight down from above - mid == end so
    // the update loop's horizontal phase is a no-op hold, not a slide.
    end = {mount.x, mount.y};
    mid = end;
    start = {mount.x, kEngineBayBounds.y - mount.h};
}

// The mount rect has to come from the part being fitted, not the one still
// on the car - a bigger block sits at a different spot, not just a bigger
// box at the same one.
void Garage::StartEngineBaySlideIn(EngineBaySlot slot, AmigaLibAsset asset, const SDL_Rect& mount) {
    int idx = (int)slot;
    BeginSlide(mount, engineBaySlideStart[idx], engineBaySlideMid[idx], engineBaySlideEnd[idx]);

    if (engineBay) {
        Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
        DialogBitmap* db = engineBay->GetDialog()->GetBitmap(SlotBitmapId(slot));
        if (db) {
            db->bitmap = bmp;
            bmp->SetVisible(true);
            SDL_Rect logical = {engineBaySlideStart[idx].x, engineBaySlideStart[idx].y, mount.w, mount.h};
            ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
        }
    }
    engineBaySlotPhase[idx] = PartInstallPhase::SLIDING_IN;
    engineBaySlideTimer[idx] = 0.0f;
}

void Garage::StartEngineSlideIn(const EnginePart& newEngine) {
    pendingEngine = newEngine;
    AmigaLibAsset asset = EngineBodyAsset(newEngine.engineType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    StartEngineBaySlideIn(EngineBaySlot::ENGINE, asset,
                          EngineBodyRect(bmp->GetPosition(), newEngine.engineType));
}

void Garage::StartManifoldSlideIn(const Part& newManifold) {
    pendingManifold = newManifold;
    ManifoldType type = newManifold.partType.manifoldType;
    AmigaLibAsset asset = ManifoldBodyAsset(type);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    StartEngineBaySlideIn(EngineBaySlot::MANIFOLD, asset, ManifoldBodyRect(bmp->GetPosition(), type));
}

void Garage::StartCarbSlideIn(const Part& newCarb) {
    pendingCarb = newCarb;
    Car* car = state.GetCurrentCar();
    if (!car) return;
    AmigaLibAsset asset = CarbBodyAsset(newCarb.partType.carburetorType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    // Where a carb sits is decided by the manifold under it, not the carb.
    StartEngineBaySlideIn(EngineBaySlot::CARB, asset,
                          CarbBodyRect(bmp->GetPosition(), ResolveManifoldType(*car), ResolveEngineType(*car)));
}

void Garage::RefreshEngineBayGeometry() {
    if (!engineBay) return;
    engineBay->RefreshPartsForCurrentCar();
    // That re-seats every screw; put back the state each one is actually
    // in, since another slot can be mid-disassembly.
    for (int i = 0; i < 4; i++) UpdateEngineBayScrewVisual(i);
}

// The old part was already handed to spares in ReleaseEngineBayPartToSpares
// when the slot was unscrewed - this just installs the new one. The refresh
// afterwards re-seats everything the new part moves: a different block
// shifts its own screw and ground wire, and a different manifold shifts the
// carburetor sitting on it.
void Garage::CommitEngineSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    state.RemoveSpareEngine(pendingEngine.engineType);
    car->SetEngine(pendingEngine);
    RefreshEngineBayGeometry();
}

void Garage::CommitManifoldSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    state.RemoveSpareManifold(pendingManifold.partType.manifoldType);
    car->SetManifold(pendingManifold);
    RefreshEngineBayGeometry();
}

void Garage::CommitCarbSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    state.RemoveSpareCarburetor(pendingCarb.partType.carburetorType);
    car->SetCarburator(pendingCarb);
    RefreshEngineBayGeometry();
}

void Garage::UpdateEngineBayInstallAnimation(double deltaTime) {
    UpdateEngineBayScrewFallAnimations(deltaTime);
    if (!engineBay) return;
    Dialog* dlg = engineBay->GetDialog();
    if (!dlg) return;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto clamp01 = [](float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); };

    for (int idx = 0; idx < 3; idx++) {
        if (engineBaySlotPhase[idx] != PartInstallPhase::SLIDING_IN) continue;
        EngineBaySlot slot = (EngineBaySlot)idx;
        DialogBitmap* db = dlg->GetBitmap(SlotBitmapId(slot));
        if (!db) continue;

        engineBaySlideTimer[idx] += (float)deltaTime;
        SDL_Rect mount = db->bitmap ? db->bitmap->GetPosition() : SDL_Rect{0, 0, 0, 0};
        int logicalX;
        int logicalY;
        if (engineBaySlideTimer[idx] < kSlideUpDuration) {
            float t = clamp01(engineBaySlideTimer[idx] / kSlideUpDuration);
            logicalX = engineBaySlideStart[idx].x;
            logicalY = (int)lerp((float)engineBaySlideStart[idx].y, (float)engineBaySlideMid[idx].y, t);
        } else {
            float t = clamp01((engineBaySlideTimer[idx] - kSlideUpDuration) / kSlideRightDuration);
            logicalY = engineBaySlideMid[idx].y;
            logicalX = (int)lerp((float)engineBaySlideMid[idx].x, (float)engineBaySlideEnd[idx].x, t);
        }

        SDL_Rect logical = {logicalX, logicalY, mount.w, mount.h};
        ClipDialogBitmapToRect(*db, kEngineBayMask, logical);

        if (engineBaySlideTimer[idx] >= kSlideUpDuration + kSlideRightDuration) {
            logical = {engineBaySlideEnd[idx].x, engineBaySlideEnd[idx].y, mount.w, mount.h};
            ClipDialogBitmapToRect(*db, kEngineBayMask, logical);

            if (engineBaySlotRejected[idx]) {
                // Doesn't fit - the original still shows it sliding into the
                // mount before bouncing it back out, rather than blocking
                // the pick up front. No screws, no commit, no spares change
                // - the part just goes back to being a spare.
                engineBaySlotRejected[idx] = false;
                db->bitmap->SetVisible(false);
                engineBaySlotPhase[idx] = PartInstallPhase::EMPTY;
                bubbles[12]->SetVisible(true);  // partIncompatibleBubble
                continue;
            }

            engineBaySlotPhase[idx] = PartInstallPhase::SCREWING_IN;
            int first = SlotFirstScrew(slot);
            int count = SlotScrewCount(slot);
            for (int i = 0; i < count; i++) {
                int screwIdx = first + i;
                engineBayScrewState[screwIdx] = 2;
                StartEngineBayScrewFall(screwIdx, /*fallingOut=*/false, /*delay=*/i * kScrewFallDuration);
            }
            if (slot == EngineBaySlot::ENGINE) {
                // Fresh engine drops in with its wire unplugged too.
                engineWireAttached = false;
                DialogBitmap* wireDb = dlg->GetBitmap("engine_wire");
                if (wireDb) {
                    wireDb->bitmap = state.GetAssets().GetBitmapAsset(
                        {LibAssetRegular, SrLibFileAmigaLibP, ENGINE_WIRE_DETACHED});
                }
            }
        }
    }
}
