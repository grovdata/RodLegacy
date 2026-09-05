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

// screwIndex 0=engine, 1=manifold-left, 2=manifold-right, 3..5=carb 0..2.
static EngineBaySlot ScrewSlot(int screwIndex) {
    if (screwIndex == 0) return EngineBaySlot::ENGINE;
    if (screwIndex == 1 || screwIndex == 2) return EngineBaySlot::MANIFOLD;
    return (EngineBaySlot)(screwIndex - 1);
}

static int SlotFirstScrew(EngineBaySlot slot) { return EngineBay::GetFirstScrew((int)slot); }
static int SlotScrewCount(EngineBaySlot slot) { return EngineBay::GetScrewCount((int)slot); }
static bool IsCarbSlot(EngineBaySlot slot) { return (int)slot >= (int)EngineBaySlot::CARB0; }
static int CarbIndex(EngineBaySlot slot) { return (int)slot - (int)EngineBaySlot::CARB0; }

static void SetSlotVisible(EngineBay* bay, EngineBaySlot slot, bool visible) {
    if (bay) bay->SetPartVisible((int)slot, visible);
}

// Screw mount point: x is the sprite's left edge, y its bottom edge.
static SDL_Point ScrewPos(EngineBay* bay, int screwIndex) {
    return bay ? bay->GetScrewAnchor(screwIndex) : SDL_Point{0, 0};
}

// ENGINE_BOLT is one image, not a frame sheet: the game crops it from the
// top and blits it bottom-aligned on the mount, so backing a screw out grows
// it upwards a third at a time - seated, half out, all the way out.
static SDL_Rect ScrewFrameCrop(const SDL_Point& natural, int state) {
    int frameH = natural.y / 3;
    return {0, 0, natural.x, frameH * (state + 1)};
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

    for (int i = 0; i < kEngineBayScrewCount; i++) {
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

// The part really leaves the car and lands in spares, so it is available in
// the picker straight away and the car stops running until something is put
// back - the original nulls the matching pointer on its car struct.
void Garage::ReleaseEngineBayPartToSpares(EngineBaySlot slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (IsCarbSlot(slot)) {
        int idx = CarbIndex(slot);
        if (!car->HasCarburetor(idx)) return;
        state.AddSpareCarburetor(car->GetCarburetor(idx));
        car->RemoveCarburetor(idx);
        return;
    }
    if (slot == EngineBaySlot::ENGINE) {
        if (!car->HasEngine()) return;
        EnginePart old;
        if (car->GetSpecEngineClass() >= 0) {
            CarEngine_t type = car->GetEngineType();
            old = EnginePart(type, 0, 0, car->GetModelRef().vendor, EngineBodyAsset(type));
        } else {
            old = car->GetEngine();
        }
        state.AddSpareEngine(old);
        car->RemoveEngine();
        return;
    }
    if (!car->HasManifold()) return;
    Part old = car->GetSpecManifoldClass() >= 0
        ? CarBuilder::CreateManifoldPart(car->GetManifoldType(), car->GetModelRef().vendor)
        : car->GetManifold();
    state.AddSpareManifold(old);
    car->RemoveManifold();
}

// A slot comes apart: its part goes back to spares and its screws are gone.
// The manifold bolts to the block and the carbs bolt to the manifold, so
// pulling the engine brings the whole stack out with it - each piece then
// has to be picked and fitted again on its own.
void Garage::EmptyEngineBaySlot(EngineBaySlot slot) {
    int idx = (int)slot;
    if (engineBaySlotPhase[idx] == PartInstallPhase::EMPTY) return;

    // Top of the stack downwards: every carburetor bolts to the manifold and
    // the manifold to the block, so they come off before whatever holds
    // them, and the manifold is still known while its carbs are counted.
    if (slot == EngineBaySlot::ENGINE || slot == EngineBaySlot::MANIFOLD) {
        for (int i = kMaxCarburetors - 1; i >= 0; i--) {
            EmptyEngineBaySlot((EngineBaySlot)((int)EngineBaySlot::CARB0 + i));
        }
    }
    if (slot == EngineBaySlot::ENGINE) EmptyEngineBaySlot(EngineBaySlot::MANIFOLD);

    SetSlotVisible(engineBay.get(), slot, false);
    ReleaseEngineBayPartToSpares(slot);
    engineBaySlotPhase[idx] = PartInstallPhase::EMPTY;

    int first = SlotFirstScrew(slot);
    for (int i = first; i < first + SlotScrewCount(slot); i++) {
        engineBayScrewState[i] = 2;
        engineBayScrewFalling[i] = false;
        engineBayScrewFallDelay[i] = 0.0f;
        if (engineBay->GetScrewBitmap(i)) engineBay->GetScrewBitmap(i)->SetVisible(false);
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

        if (IsEngineBaySlotFullyOut(slot)) EmptyEngineBaySlot(slot);
    } else if (engineBaySlotPhase[slotIdx] == PartInstallPhase::SCREWING_IN) {
        if (engineBayScrewState[screwIndex] > 0) engineBayScrewState[screwIndex]--;
        UpdateEngineBayScrewVisual(screwIndex);

        if (IsEngineBaySlotFullySeated(slot)) {
            CommitEngineBaySwap(slot);
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
        EmptyEngineBaySlot(slot);
    } else if (engineBaySlotPhase[slotIdx] == PartInstallPhase::SCREWING_IN && IsEngineBaySlotFullySeated(slot)) {
        CommitEngineBaySwap(slot);
        engineBaySlotPhase[slotIdx] = PartInstallPhase::MOUNTED;
    }
}

void Garage::OnEngineBayPartClicked(EngineBaySlot slot) {
    if (engineBaySlotPhase[(int)slot] != PartInstallPhase::EMPTY) return;
    Car* car = state.GetCurrentCar();
    // A mount this manifold doesn't have isn't a slot you can fill.
    if (IsCarbSlot(slot) && car && CarbIndex(slot) >= car->GetCarburetorSlots()) return;
    // The stack goes back in the order it came out - a manifold has nothing
    // to bolt to without a block, and a carb has nothing without a manifold.
    if (slot != EngineBaySlot::ENGINE) {
        EngineBaySlot under = IsCarbSlot(slot) ? EngineBaySlot::MANIFOLD : EngineBaySlot::ENGINE;
        if (engineBaySlotPhase[(int)under] != PartInstallPhase::MOUNTED) {
            if (hud) {
                hud->SetText(under == EngineBaySlot::ENGINE ? "Fit an engine first"
                                                            : "Fit a manifold first");
            }
            return;
        }
    }
    if (slot == EngineBaySlot::ENGINE) ShowEngineDialog();
    else if (slot == EngineBaySlot::MANIFOLD) ShowManifoldDialog();
    else ShowCarburetorDialog(CarbIndex(slot));
}

// The pre-existing "Parts" button used to open a hardcoded sample list
// unrelated to the car. Now it's a second way into whichever slot is
// currently disassembled (the main way is clicking the emptied part
// itself - see OnEngineBayPartClicked/part_N zones). Walking the slots in
// order is what makes a stripped engine go back together one part at a time.
void Garage::OnEngineBayPartsButtonClicked() {
    for (int i = 0; i < kEngineBayPartSlots; i++) {
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
    if (id.rfind("part_", 0) == 0) {
        OnEngineBayPartClicked((EngineBaySlot)std::stoi(id.substr(5)));
    }
}

static void BeginSlide(const SDL_Rect& mount, SDL_Point& start, SDL_Point& end) {
    // All engine bay parts drop straight down from above, so only y moves.
    end = {mount.x, mount.y};
    start = {mount.x, kEngineBayBounds.y - mount.h};
}

void Garage::ApplyEngineBaySlideFrame(EngineBaySlot slot, int y, const SDL_Rect& mount) {
    if (!engineBay) return;
    Dialog* dlg = engineBay->GetDialog();
    if (!dlg) return;
    DialogBitmap* db = dlg->GetBitmap(EngineBay::GetPartBitmapId((int)slot));
    if (db) ClipDialogBitmapToRect(*db, kEngineBayMask, {mount.x, y, mount.w, mount.h});
}

// The mount rect has to come from the part being fitted, not the one still
// on the car - a bigger block sits at a different spot, not just a bigger
// box at the same one.
void Garage::StartEngineBaySlideIn(EngineBaySlot slot, AmigaLibAsset asset, const SDL_Rect& mount) {
    int idx = (int)slot;
    BeginSlide(mount, engineBaySlideStart[idx], engineBaySlideEnd[idx]);

    if (engineBay) {
        engineBay->SetPartAsset(idx, asset);
        engineBay->SetPartVisible(idx, true);
        ApplyEngineBaySlideFrame(slot, engineBaySlideStart[idx].y, mount);
    }
    engineBaySlotPhase[idx] = PartInstallPhase::SLIDING_IN;
    engineBaySlideTimer[idx] = 0.0f;
}

void Garage::StartEngineSlideIn(const EnginePart& newEngine) {
    pendingEngine = newEngine;
    AmigaLibAsset asset = EngineBodyAsset(newEngine.engineType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    StartEngineBaySlideIn(EngineBaySlot::ENGINE, asset,
                          EngineBodyRect(bmp->GetNaturalSize(), newEngine.engineType));
}

void Garage::StartManifoldSlideIn(const Part& newManifold) {
    pendingManifold = newManifold;
    ManifoldType type = newManifold.partType.manifoldType;
    AmigaLibAsset asset = ManifoldBodyAsset(type);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    StartEngineBaySlideIn(EngineBaySlot::MANIFOLD, asset, ManifoldBodyRect(bmp->GetNaturalSize(), type));
}

void Garage::StartCarbSlideIn(int carbIndex, const Part& newCarb) {
    Car* car = state.GetCurrentCar();
    if (!car || carbIndex < 0 || carbIndex >= kMaxCarburetors) return;
    pendingCarb[carbIndex] = newCarb;
    AmigaLibAsset asset = CarbBodyAsset(newCarb.partType.carburetorType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    // Where a carb sits is decided by the manifold under it, not the carb.
    StartEngineBaySlideIn(
        (EngineBaySlot)((int)EngineBaySlot::CARB0 + carbIndex), asset,
        CarbBodyRect(bmp->GetNaturalSize(), car->GetManifoldType(), car->GetEngineType(), carbIndex));
}

// Dialog::Show() turns every bitmap in the dialog back on, and the bay is
// re-shown every time a picker closes - so anything currently apart has to
// be hidden again afterwards. Also the tail of a refresh, which reassembles
// the bay wholesale: fitting an engine leaves the manifold and carbs out
// until they are fitted too.
void Garage::ApplyEngineBayAssemblyState() {
    if (!engineBay) return;
    for (int idx = 0; idx < kEngineBayPartSlots; idx++) {
        EngineBaySlot slot = (EngineBaySlot)idx;
        PartInstallPhase phase = engineBaySlotPhase[idx];

        // Only a missing part is hidden - one still sliding in is on screen
        // even though the car doesn't own it yet.
        SetSlotVisible(engineBay.get(), slot, phase != PartInstallPhase::EMPTY);

        // SCREWING_IN screws are mid drop-in - the fall animation owns them.
        if (phase == PartInstallPhase::SCREWING_IN) continue;
        int first = SlotFirstScrew(slot);
        for (int i = first; i < first + SlotScrewCount(slot); i++) {
            bool showing = phase != PartInstallPhase::EMPTY &&
                           phase != PartInstallPhase::SLIDING_IN &&
                           engineBayScrewState[i] < 2;
            if (Bitmap* bmp = engineBay->GetScrewBitmap(i)) bmp->SetVisible(showing);
            if (showing) UpdateEngineBayScrewVisual(i);
        }
    }
}

// The phases mirror what the car actually has fitted. A manifold swap can
// change how many carburetor mounts exist, and loading a save or driving in
// a different car replaces the lot, so slots sitting at rest are re-derived
// from the car; anything mid-animation is left to finish.
void Garage::SyncEngineBayPhasesFromCar() {
    Car* car = state.GetCurrentCar();
    if (!car || !engineBay) return;
    for (int i = 0; i < kEngineBayPartSlots; i++) {
        if (engineBaySlotPhase[i] != PartInstallPhase::MOUNTED &&
            engineBaySlotPhase[i] != PartInstallPhase::EMPTY) {
            continue;
        }
        bool fitted = engineBay->IsPartFitted(i);
        engineBaySlotPhase[i] = fitted ? PartInstallPhase::MOUNTED : PartInstallPhase::EMPTY;
        EngineBaySlot slot = (EngineBaySlot)i;
        int first = SlotFirstScrew(slot);
        for (int s = first; s < first + SlotScrewCount(slot); s++) {
            engineBayScrewState[s] = fitted ? 0 : 2;
        }
        if (slot == EngineBaySlot::ENGINE) engineWireAttached = fitted;
    }
}

void Garage::RefreshEngineBayGeometry() {
    if (!engineBay) return;
    engineBay->RefreshPartsForCurrentCar();
    SyncEngineBayPhasesFromCar();
    ApplyEngineBayAssemblyState();
}

// The old part was already handed to spares in ReleaseEngineBayPartToSpares
// when the slot was unscrewed - this just installs the new one. The refresh
// afterwards re-seats everything the new part moves: a different block
// shifts its own screw and ground wire, and a different manifold shifts the
// carburetor sitting on it.
void Garage::CommitEngineBaySwap(EngineBaySlot slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (slot == EngineBaySlot::ENGINE) {
        state.RemoveSpareEngine(pendingEngine.engineType);
        car->SetEngine(pendingEngine);
    } else if (slot == EngineBaySlot::MANIFOLD) {
        state.RemoveSpareManifold(pendingManifold.partType.manifoldType);
        // SetManifold would fill every mount with a carb; this manifold
        // arrives bare and its carbs get fitted one at a time after it.
        car->SetManifold(pendingManifold);
        for (int i = 0; i < kMaxCarburetors; i++) car->RemoveCarburetor(i);
    } else {
        int idx = CarbIndex(slot);
        state.RemoveSpareCarburetor(pendingCarb[idx].partType.carburetorType);
        car->SetCarburetor(idx, pendingCarb[idx]);
    }
    RefreshEngineBayGeometry();
}

void Garage::UpdateEngineBayInstallAnimation(double deltaTime) {
    UpdateEngineBayScrewFallAnimations(deltaTime);
    if (!engineBay) return;
    Dialog* dlg = engineBay->GetDialog();
    if (!dlg) return;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto clamp01 = [](float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); };

    for (int idx = 0; idx < kEngineBayPartSlots; idx++) {
        if (engineBaySlotPhase[idx] != PartInstallPhase::SLIDING_IN) continue;
        EngineBaySlot slot = (EngineBaySlot)idx;
        DialogBitmap* db = dlg->GetBitmap(EngineBay::GetPartBitmapId(idx));
        if (!db || !db->bitmap) continue;

        engineBaySlideTimer[idx] += (float)deltaTime;
        SDL_Rect mount = db->bitmap->GetNaturalSize();
        mount.x = engineBaySlideEnd[idx].x;
        float t = clamp01(engineBaySlideTimer[idx] / kSlideUpDuration);
        int logicalY = (int)lerp((float)engineBaySlideStart[idx].y, (float)engineBaySlideEnd[idx].y, t);
        ApplyEngineBaySlideFrame(slot, logicalY, mount);

        if (engineBaySlideTimer[idx] >= kSlideUpDuration + kSlideRightDuration) {
            ApplyEngineBaySlideFrame(slot, engineBaySlideEnd[idx].y, mount);

            if (engineBaySlotRejected[idx]) {
                // Doesn't fit - the original still shows it sliding into the
                // mount before bouncing it back out, rather than blocking
                // the pick up front. No screws, no commit, no spares change
                // - the part just goes back to being a spare.
                engineBaySlotRejected[idx] = false;
                SetSlotVisible(engineBay.get(), slot, false);
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
