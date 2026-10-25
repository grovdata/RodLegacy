#include "garage/garage.h"
#include "car/engine_bay_layout.h"
#include "core/state.h"
#include "ui/hud.h"
#include "core/assets.h"
#include "parsers/libp.h"
#include "car/car_builder.h"
#include <algorithm>

using EngineBaySlot = Garage::EngineBaySlot;
using PartInstallPhase = Garage::PartInstallPhase;

// Same bounds as EngineBay's overview, so slide-ins stay inside the window art.
static const SDL_Rect kEngineBayBounds = {24, 0, 200, 189};
static const SDL_Rect kEngineBayMask = {28, 4, 193, 181};

static constexpr float kScrewFallDuration = 0.35f;
static constexpr float kSlideUpDuration = 1.1f;
static constexpr float kSlideRightDuration = 0.4f;

// Two pitch bands, fresh every time: random % 50 + 400 at volume 60, random % 50 + 600 at volume 40.
namespace {
// SR1's bay clock costs (0x241f20, 0x242018, 0x2420a8 on fitting; 0x242422, 0x242458, 0x24249a on removal): a part that
// does not fit costs a third more.
uint32_t BaySlotTicks(EngineBaySlot slot) {
    if (slot == EngineBaySlot::ENGINE) return 900;
    if (slot == EngineBaySlot::MANIFOLD) return 450;
    return 225;
}
}

void Garage::PlayBayToolSound(bool seating) {
    if (!audioManager) return;
    const int period = (seating ? SRL0_PART_MOUNT_PERIOD_BASE : SRL0_PART_MOUNT_PERIOD_HIGH) +
                       (std::rand() % SRL0_PART_MOUNT_PERIOD_SPREAD);
    const int volume = seating ? SRL0_PART_MOUNT_VOLUME_LOW : SRL0_PART_MOUNT_VOLUME_HIGH;
    audioManager->PlaySound(AudioSampleType::PART_MOUNT, volume / 64.0f, 3579545 / period);
}

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

// Every install and removal knocks the timing to a random one of the 13 positions, as in the original.
static void ShakeTuning(Car* car, EngineBay* bay) {
    if (!car) return;
    std::uniform_int_distribution<int> knob(0, kTuneLevelCount - 1);
    int level = knob(RandomEngine());
    car->SetTuneLevel(level);
    // EngineBay keeps its own copy of the level.
    if (bay) bay->SetKnobPosition(level);
}

// Screw mount point: x is the sprite's left edge, y its bottom edge.
static SDL_Point ScrewPos(EngineBay* bay, int screwIndex) {
    return bay ? bay->GetScrewAnchor(screwIndex) : SDL_Point{0, 0};
}

// ENGINE_BOLT is one image cropped from the top and bottom-aligned, so backing out grows it a third at a time.
static SDL_Rect ScrewFrameCrop(const SDL_Point& natural, int state) {
    int frameH = natural.y / 3;
    return {0, 0, natural.x, frameH * (state + 1)};
}

// Anchored at the bottom: the crop shrinks as the screw is driven in.
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

// The engine slot also needs its ground wire unplugged.
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

// The part really leaves the car for spares, and the car stops running until something is put back.
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

// Pulling the engine brings the manifold and carbs out with it.
void Garage::EmptyEngineBaySlot(EngineBaySlot slot) {
    int idx = (int)slot;
    if (engineBaySlotPhase[idx] == PartInstallPhase::EMPTY) return;

    ShakeTuning(state.GetCurrentCar(), engineBay.get());

    // Top of the stack downwards, while the manifold is still known for counting its carbs.
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
        engineBayScrewFallDelay[i] = 0.0f;
        // Left to finish: cancelling it made a one-screw slot's screw vanish instead of falling.
        if (engineBayScrewFalling[i] && engineBayScrewFallingOut[i]) continue;
        engineBayScrewFalling[i] = false;
        if (engineBay->GetScrewBitmap(i)) engineBay->GetScrewBitmap(i)->SetVisible(false);
    }
}

void Garage::OnEngineBayScrewClicked(int screwIndex) {
    EngineBaySlot slot = ScrewSlot(screwIndex);
    int slotIdx = (int)slot;
    // Every turn of a screw on a fitted part, either way (SR1 0x242336).
    if (engineBaySlotPhase[slotIdx] != PartInstallPhase::EMPTY) state.AdvanceTicks(29);

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::MOUNTED) {
        engineBaySlotPhase[slotIdx] = PartInstallPhase::UNSCREWING;
    }

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::UNSCREWING) {
        if (engineBayScrewState[screwIndex] < 2) engineBayScrewState[screwIndex]++;
        UpdateEngineBayScrewVisual(screwIndex);
        PlayBayToolSound(false);
        if (engineBayScrewState[screwIndex] == 2) {
            StartEngineBayScrewFall(screwIndex, /*fallingOut=*/true);
        }

        if (IsEngineBaySlotFullyOut(slot)) {
            state.AdvanceTicks(BaySlotTicks(slot));
            EmptyEngineBaySlot(slot);
        }
    } else if (engineBaySlotPhase[slotIdx] == PartInstallPhase::SCREWING_IN) {
        if (engineBayScrewState[screwIndex] > 0) engineBayScrewState[screwIndex]--;
        UpdateEngineBayScrewVisual(screwIndex);
        PlayBayToolSound(false);

        if (IsEngineBaySlotFullySeated(slot)) {
            CommitEngineBaySwap(slot);
            engineBaySlotPhase[slotIdx] = PartInstallPhase::MOUNTED;
        }
    }
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
    ApplyEngineWireVisual();
    // Plugging or pulling the wire, either way (SR1 0x241d5c / 0x241d7c).
    state.AdvanceTicks(59);

    if (engineBaySlotPhase[slotIdx] == PartInstallPhase::UNSCREWING && IsEngineBaySlotFullyOut(slot)) {
        state.AdvanceTicks(BaySlotTicks(slot));
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
    // Back in the order it came out.
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

// A second way into the currently disassembled slot, walking the slots in order.
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

// From the part being fitted: a bigger block sits at a different spot.
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
    if (engineBay) engineBay->SetPendingEngine(newEngine.engineType);
    AmigaLibAsset asset = EngineBodyAsset(newEngine.engineType);
    Bitmap* bmp = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, asset});
    StartEngineBaySlideIn(EngineBaySlot::ENGINE, asset,
                          EngineBodyRect(bmp->GetNaturalSize(), newEngine.engineType));
}

void Garage::StartManifoldSlideIn(const Part& newManifold) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    pendingManifold = newManifold;
    ManifoldType type = newManifold.partType.manifoldType;
    // The 2-barrel body it slides in as depends on the block it lands on.
    AmigaLibAsset asset = ManifoldBodyAsset(type, car->GetEngineType());
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

// Re-applied per car, or a car bought while the last one's engine was out drives in unplugged.
void Garage::ApplyEngineWireVisual() {
    if (!engineBay || !engineBay->GetDialog()) return;
    DialogBitmap* db = engineBay->GetDialog()->GetBitmap("engine_wire");
    if (!db) return;
    db->bitmap = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP,
         engineWireAttached ? ENGINE_WIRE_ATTACHED : ENGINE_WIRE_DETACHED});
    // The two arts differ in height and the dialog stretches a bitmap to its rect.
    const SDL_Rect natural = db->bitmap->GetNaturalSize();
    db->rect.w = natural.w;
    db->rect.h = natural.h;
    if (Zone* z = engineBay->GetDialog()->GetZone("wire_0")) z->SetRect(db->rect);
}

// Dialog::Show() turns every bitmap on, so whatever is apart is hidden again.
void Garage::ApplyEngineBayAssemblyState() {
    if (!engineBay) return;
    ApplyEngineWireVisual();
    for (int idx = 0; idx < kEngineBayPartSlots; idx++) {
        EngineBaySlot slot = (EngineBaySlot)idx;
        PartInstallPhase phase = engineBaySlotPhase[idx];

        // A part sliding in is shown even though the car doesn't own it yet.
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

// Slots at rest are re-derived from the car; anything mid-animation is left to finish.
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

// The old part already went to spares. The refresh re-seats everything the new part moves.
void Garage::CommitEngineBaySwap(EngineBaySlot slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (slot == EngineBaySlot::ENGINE) {
        state.RemoveSpareEngine(pendingEngine.engineType);
        car->SetEngine(pendingEngine);
        if (engineBay) engineBay->ClearPendingEngine();
    } else if (slot == EngineBaySlot::MANIFOLD) {
        state.RemoveSpareManifold(pendingManifold.partType.manifoldType);
        // SetManifold would fill every mount; this one arrives bare.
        car->SetManifold(pendingManifold);
        for (int i = 0; i < kMaxCarburetors; i++) car->RemoveCarburetor(i);
    } else {
        int idx = CarbIndex(slot);
        state.RemoveSpareCarburetor(pendingCarb[idx].partType.carburetorType);
        car->SetCarburetor(idx, pendingCarb[idx]);
    }
    ShakeTuning(car, engineBay.get());
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
                // Doesn't fit: the original shows it sliding in before bouncing it out. The part stays a spare.
                engineBaySlotRejected[idx] = false;
                if (slot == EngineBaySlot::ENGINE && engineBay) engineBay->ClearPendingEngine();
                SetSlotVisible(engineBay.get(), slot, false);
                engineBaySlotPhase[idx] = PartInstallPhase::EMPTY;
                bubbles[12]->SetVisible(true);  // partIncompatibleBubble
                state.AdvanceTicks(BaySlotTicks(slot) * 4 / 3);
                continue;
            }

            // The part is home: the low, loud roll of the tool sound.
            PlayBayToolSound(true);
            state.AdvanceTicks(BaySlotTicks(slot));
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
                ApplyEngineWireVisual();
            }
        }
    }
}
