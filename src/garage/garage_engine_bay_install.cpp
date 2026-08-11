#include "garage.h"
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

// Fixed display box per part - matches engine_bay.cpp's own static rects.
// Unlike the transmission (which sizes to the bitmap's natural size), these
// stretch whatever asset is loaded into a fixed box, same convention the
// overview screen already used for its old hardcoded placeholders.
static SDL_Rect SlotMountRect(EngineBaySlot slot) {
    switch (slot) {
        case EngineBaySlot::ENGINE: return {62, 51, 136, 112};
        case EngineBaySlot::MANIFOLD: return {93, 52, 88, 31};
        case EngineBaySlot::CARB: return {116, 35, 32, 29};
    }
    return {0, 0, 0, 0};
}

// Matches engine_bay.cpp's engineScrewRect/manifoldLeftScrewRect/
// manifoldRightScrewRect/carbScrewRect - kept in sync manually since the
// zones live in that file and the geometry lives here.
static SDL_Point ScrewPos(int screwIndex) {
    switch (screwIndex) {
        case 0: return {126, 165};
        case 1: return {90, 48};
        case 2: return {170, 48};
        default: return {130, 32};
    }
}

// ENGINE_BOLT is a real 3-frame sheet (one 8x4 frame per row) - state
// selects a frame, not a growing crop like the transmission's screws.
static SDL_Rect ScrewFrameCrop(const SDL_Point& natural, int state) {
    int frameH = natural.y / 3;
    return {0, state * frameH, natural.x, frameH};
}

void Garage::ApplyEngineBayScrewGeometry(int screwIndex, int y) {
    if (!engineBay) return;
    Dialog* dlg = engineBay->GetDialog();
    if (!dlg) return;
    DialogBitmap* db = dlg->GetBitmap("screw_" + std::to_string(screwIndex));
    if (!db) return;

    SDL_Point natural = engineBay->GetScrewNaturalSize(screwIndex);
    SDL_Rect crop = ScrewFrameCrop(natural, engineBayScrewState[screwIndex]);
    SDL_Point pos = ScrewPos(screwIndex);
    SDL_Rect logical = {pos.x, y, crop.w, crop.h};
    ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
}

void Garage::UpdateEngineBayScrewVisual(int screwIndex) {
    ApplyEngineBayScrewGeometry(screwIndex, ScrewPos(screwIndex).y);
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

    SDL_Point pos = ScrewPos(screwIndex);
    SDL_Point natural = engineBay->GetScrewNaturalSize(screwIndex);
    int cropH = ScrewFrameCrop(natural, engineBayScrewState[screwIndex]).h;

    engineBayScrewFalling[screwIndex] = true;
    engineBayScrewFallingOut[screwIndex] = fallingOut;
    engineBayScrewFallTimer[screwIndex] = 0.0f;
    if (fallingOut) {
        engineBayScrewFallFromY[screwIndex] = pos.y;
        engineBayScrewFallToY[screwIndex] = kEngineBayBounds.y + kEngineBayBounds.h + cropH;
    } else {
        engineBayScrewFallFromY[screwIndex] = kEngineBayBounds.y - cropH;
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

static void BeginSlide(EngineBaySlot slot, SDL_Point& start, SDL_Point& mid, SDL_Point& end) {
    SDL_Rect mount = SlotMountRect(slot);
    end = {mount.x, mount.y};
    mid = {mount.x - 16, mount.y};
    start = {mount.x - 16, kEngineBayBounds.y + kEngineBayBounds.h};
}

void Garage::StartEngineSlideIn(const EnginePart& newEngine) {
    pendingEngine = newEngine;
    int idx = (int)EngineBaySlot::ENGINE;
    BeginSlide(EngineBaySlot::ENGINE, engineBaySlideStart[idx], engineBaySlideMid[idx], engineBaySlideEnd[idx]);

    if (engineBay) {
        Bitmap* bmp = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, EngineBodyAsset(newEngine.engineType)});
        DialogBitmap* db = engineBay->GetDialog()->GetBitmap("engine_body");
        if (db) {
            db->bitmap = bmp;
            bmp->SetVisible(true);
            SDL_Rect mount = SlotMountRect(EngineBaySlot::ENGINE);
            SDL_Rect logical = {engineBaySlideStart[idx].x, engineBaySlideStart[idx].y, mount.w, mount.h};
            ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
        }
    }
    engineBaySlotPhase[idx] = PartInstallPhase::SLIDING_IN;
    engineBaySlideTimer[idx] = 0.0f;
}

void Garage::StartManifoldSlideIn(const Part& newManifold) {
    pendingManifold = newManifold;
    int idx = (int)EngineBaySlot::MANIFOLD;
    BeginSlide(EngineBaySlot::MANIFOLD, engineBaySlideStart[idx], engineBaySlideMid[idx], engineBaySlideEnd[idx]);

    if (engineBay) {
        Bitmap* bmp = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, ManifoldBodyAsset(newManifold.partType.manifoldType)});
        DialogBitmap* db = engineBay->GetDialog()->GetBitmap("manifold_body");
        if (db) {
            db->bitmap = bmp;
            bmp->SetVisible(true);
            SDL_Rect mount = SlotMountRect(EngineBaySlot::MANIFOLD);
            SDL_Rect logical = {engineBaySlideStart[idx].x, engineBaySlideStart[idx].y, mount.w, mount.h};
            ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
        }
    }
    engineBaySlotPhase[idx] = PartInstallPhase::SLIDING_IN;
    engineBaySlideTimer[idx] = 0.0f;
}

void Garage::StartCarbSlideIn(const Part& newCarb) {
    pendingCarb = newCarb;
    int idx = (int)EngineBaySlot::CARB;
    BeginSlide(EngineBaySlot::CARB, engineBaySlideStart[idx], engineBaySlideMid[idx], engineBaySlideEnd[idx]);

    if (engineBay) {
        Bitmap* bmp = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, CarbBodyAsset(newCarb.partType.carburetorType)});
        DialogBitmap* db = engineBay->GetDialog()->GetBitmap("carb_body");
        if (db) {
            db->bitmap = bmp;
            bmp->SetVisible(true);
            SDL_Rect mount = SlotMountRect(EngineBaySlot::CARB);
            SDL_Rect logical = {engineBaySlideStart[idx].x, engineBaySlideStart[idx].y, mount.w, mount.h};
            ClipDialogBitmapToRect(*db, kEngineBayMask, logical);
        }
    }
    engineBaySlotPhase[idx] = PartInstallPhase::SLIDING_IN;
    engineBaySlideTimer[idx] = 0.0f;
}

void Garage::CommitEngineSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    // A spec-based car (never had this part explicitly fitted) has no real
    // EnginePart to hand back - GetEngine() would return garbage. Build one
    // from the resolved spec class instead (see engine_bay.cpp's
    // ResolveEngineType/ResolveManifoldType for why this gap exists).
    EnginePart old;
    if (car->GetSpecEngineClass() >= 0) {
        CarEngine_t type = ResolveEngineType(*car);
        old = EnginePart(type, 0, 0, car->GetModelRef().vendor, EngineBodyAsset(type));
    } else {
        old = car->GetEngine();
    }
    state.AddSpareEngine(old);
    state.RemoveSpareEngine(pendingEngine.engineType);
    car->SetEngine(pendingEngine);
}

void Garage::CommitManifoldSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    Part old = car->GetSpecManifoldClass() >= 0
        ? CarBuilder::CreateManifoldPart(ResolveManifoldType(*car), car->GetModelRef().vendor)
        : car->GetManifold();
    state.AddSpareManifold(old);
    state.RemoveSpareManifold(pendingManifold.partType.manifoldType);
    car->SetManifold(pendingManifold);
}

void Garage::CommitCarbSwap() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    Part old = car->GetCarburator();
    state.AddSpareCarburetor(old);
    state.RemoveSpareCarburetor(pendingCarb.partType.carburetorType);
    car->SetCarburator(pendingCarb);
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
        SDL_Rect mount = SlotMountRect(slot);
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
