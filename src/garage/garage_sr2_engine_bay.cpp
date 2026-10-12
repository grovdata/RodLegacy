#include "core/game_text.h"
#include "garage/garage.h"
#include "car/car_builder.h"
#include "car/sr2_engine_bay_layout.h"
#include "core/state.h"
#include "scenes/newspaper.h"
#include "ui/hud.h"
#include "ui/main_font.h"
#include "ui/bubble.h"
#include "car/engine_part.h"
#include <algorithm>
#include <random>

using PartInstallPhase = Garage::PartInstallPhase;

namespace {

constexpr int kEngine = 0;
constexpr int kManifold = 1;
constexpr int kCarb0 = 2;

constexpr int kBayLeft = 8;
const SDL_Rect kBayMask = {8, 13, 304, kSr2BayClipBottom - 13};
const SDL_Rect kBayBounds = {8, 13, 304, 171};

// Controls strip buttons, from dialog 10 (0x2606fe).
const SDL_Rect kTuneRect = {49, 160, 38, 10};
const SDL_Rect kPartsRect = {136, 160, 47, 10};
const SDL_Rect kDoneRect = {232, 160, 38, 10};

// The 8x6 screw sprite is cropped to 4 wide, taller the further it backs out.
constexpr int kScrewW = 4;
// Seated shows the head only; each turn out lets the shaft show. A bolt waiting in its hole is full length.
int BoltHeight(int state) { return state == 0 ? Sr2ScrewCropHeight(0) : state == 1 ? Sr2ScrewCropHeight(1) : Sr2ScrewCropHeight(2); }

// The bay screen's copperlist loads COLOR01..09 with the car's colour ramp, so everything drawn there takes it.
Bitmap* Sr2Asset(int id) {
    Car* car = state.GetCurrentCar();
    const Asset asset = {LibAssetRegular, SrLibFileAmigaLibP, id, SrGame::SR2};
    if (!car) return state.GetAssets().GetBitmapAsset(asset);
    return state.GetAssets().GetBitmapAssetWithPalette(state.GetAssets().GetLibPData(SrGame::SR2), asset,
                                                       GetStandardCarColor(car->GetColor()));
}

int CarbClassOf(const Part& carb) {
    return static_cast<int>(carb.partType.carburetorType);
}

std::string SpareEngineName(const EnginePart& p) {
    if (!p.name.empty()) return p.name;
    if (Newspaper* np = state.GetNewspaper()) {
        std::string n = np->FindEngineCatalogName(p.engineType, p.vendor);
        if (!n.empty()) return n;
    }
    return p.GetEngineName();
}

std::string SpareManifoldName(const Part& p) {
    if (!p.name.empty()) return p.name;
    if (Newspaper* np = state.GetNewspaper()) {
        std::string n = np->FindManifoldCatalogName(p.partType.manifoldType, p.vendor);
        if (!n.empty()) return n;
    }
    return "Manifold";
}

std::string SpareCarbName(const Part& p) {
    if (!p.name.empty()) return p.name;
    switch (p.partType.carburetorType) {
        case CARB_2BARREL: return "2-brl carb. Fits all models.";
        case CARB_4BARREL: return "4-brl carb. Fits all models.";
        case CARB_4BARREL_RACING: return "4-brl racing carb. Fits all models.";
    }
    return "Carburetor";
}

std::string WearNote(int wear) {
    if (WearPercent(wear) <= 0) return "(new)";
    return "(" + std::to_string(WearPercent(wear)) + "% worn)";
}

}  // namespace

int Garage::Sr2BayScrewFirst(int slot) const {
    if (slot == kManifold) return 0;
    if (slot >= kCarb0) return 6 + 2 * (slot - kCarb0);
    return 0;
}

int Garage::Sr2BayScrewCount(int slot) const {
    if (slot == kEngine) return 0;
    if (slot == kManifold) {
        Car* car = state.GetCurrentCar();
        return car && ManifoldClass(sr2Phase[kManifold] == PartInstallPhase::SCREWING_IN
                                        ? sr2PendingManifold.partType.manifoldType
                                        : car->GetManifoldType()) == 8 ? 6 : 4;
    }
    return 2;
}

// Only the part on top of the stack can come off, and a part goes back in the order it came out.
bool Garage::Sr2BayIsTop(int slot) const {
    for (int s = slot + 1; s < kSr2BaySlots; ++s) {
        if (sr2Phase[s] != PartInstallPhase::EMPTY) return false;
    }
    return true;
}

bool Garage::Sr2BaySlotOut(int slot) const {
    const int first = Sr2BayScrewFirst(slot);
    for (int i = first; i < first + Sr2BayScrewCount(slot); ++i) {
        if (sr2Screw[i] != 2) return false;
    }
    return slot != kEngine || !sr2Wire;
}

bool Garage::Sr2BaySlotSeated(int slot) const {
    const int first = Sr2BayScrewFirst(slot);
    for (int i = first; i < first + Sr2BayScrewCount(slot); ++i) {
        if (sr2Screw[i] != 0) return false;
    }
    return slot != kEngine || sr2Wire;
}

// Everything fitted and bolted down, the wire plugged: the only time the engine can be tuned.
bool Garage::Sr2BayReadyToTune() const {
    Car* car = state.GetCurrentCar();
    if (!car || !car->IsRunning() || !sr2Wire) return false;
    for (int slot = 0; slot < kSr2BaySlots; ++slot) {
        const bool needed = slot < kCarb0 || slot - kCarb0 < car->GetCarburetorSlots();
        if (needed && sr2Phase[slot] != PartInstallPhase::MOUNTED) return false;
    }
    return true;
}

void Garage::Sr2BayDeriveFromCar() {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    const bool sameCar = sr2Car == car;
    sr2Car = car;
    for (int slot = 0; slot < kSr2BaySlots; ++slot) {
        if (sameCar && sr2Phase[slot] == PartInstallPhase::SCREWING_IN) continue;
        const bool fitted = slot == kEngine ? car->HasEngine() : slot == kManifold ? car->HasManifold()
                                                                 : car->HasCarburetor(slot - kCarb0);
        sr2Phase[slot] = fitted ? PartInstallPhase::MOUNTED : PartInstallPhase::EMPTY;
        const int first = Sr2BayScrewFirst(slot);
        const int count = slot == kEngine ? 0 : slot == kManifold ? kSr2BayScrews / 2 : 2;
        for (int i = first; i < first + count && i < kSr2BayScrews; ++i) sr2Screw[i] = fitted ? 0 : 2;
        if (slot == kEngine) sr2Wire = fitted;
    }
}

void Garage::OpenSr2EngineBay() {
    if (!state.GetCurrentCar()) return;
    sr2Tuning = false;
    sr2MachineDir = 0;
    sr2MachineX = 8;
    Sr2BayDeriveFromCar();
    Sr2BayRebuild();
    SetActiveDialog(sr2BayDialog.get());
}

void Garage::Sr2BayRebuild() {
    Car* car = state.GetCurrentCar();
    if (!car) return;

    const bool engineIn = sr2Phase[kEngine] != PartInstallPhase::EMPTY;
    const bool manifoldIn = sr2Phase[kManifold] != PartInstallPhase::EMPTY;
    const bool pendingEngine = sr2Phase[kEngine] == PartInstallPhase::SCREWING_IN;
    const bool pendingManifold = sr2Phase[kManifold] == PartInstallPhase::SCREWING_IN;

    const CarEngine_t engineType = pendingEngine ? sr2PendingEngine.engineType : car->GetEngineType();
    const int engineClass = static_cast<int>(engineType) % 3;
    const ManifoldType manifoldType = pendingManifold ? sr2PendingManifold.partType.manifoldType
                                                      : car->GetManifoldType();
    const int manifoldClass = ManifoldClass(manifoldType);
    const int slots = CarburetorsPerManifold(manifoldType);
    int carbClass = CarbClassOf(car->GetCarburator());
    for (int k = 0; k < 3; ++k) {
        if (sr2Phase[kCarb0 + k] == PartInstallPhase::SCREWING_IN) { carbClass = CarbClassOf(sr2PendingCarb[k]); break; }
    }
    const Sr2BayLayout lay = ComputeSr2BayLayout(engineClass, manifoldClass, carbClass, slots);

    DialogConfig cfg;
    cfg.bounds = kBayBounds;
    cfg.backgroundColor = {0, 0, 0, 255};
    cfg.borderColor = {0, 0, 0, 0};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.onZoneHover = [this](Zone& zone) { hud->SetText(zone.GetText().c_str()); };
    cfg.onZoneClick = [this](Zone& zone) { Sr2BayZoneClicked(zone.GetId()); };

    auto addBitmap = [&](const std::string& id, int asset, SDL_Point at, int z) {
        Bitmap* bmp = Sr2Asset(asset);
        if (!bmp) return;
        const SDL_Rect nat = bmp->GetNaturalSize();
        DialogBitmap db = {bmp, {at.x, at.y, nat.w, nat.h}, z};
        ClipDialogBitmapToRect(db, kBayMask, {at.x, at.y, nat.w, nat.h});
        cfg.bitmaps[id] = db;
    };

    cfg.bitmaps["bay"] = {Sr2Asset(SR2_ENGINE_BAY), {kBayLeft, 13, kSr2BayWidth, kSr2BayHeight}, 0};
    cfg.bitmaps["controls"] = {Sr2Asset(SR2_ENGINE_BAY_CONTROLS), {kBayLeft, 150, 304, 34}, 1};

    std::vector<Zone> zones;
    if (sr2Tuning) {
        // The two tune bolts sit at fixed offsets from the last bolt drawn: the top carburettor's, or the manifold's
        // when there is none (0x26249a: carb, body, supercharger rows).
        static const int kTuneOff[3][4] = {{-17, -18, 1, -22}, {-36, 0, -14, -3}, {-46, -20, -28, -20}};
        SDL_Point last = {0, 0};
        int row = 1;
        bool found = false;
        for (int k = slots - 1; k >= 0 && !found; --k) {
            if (lay.carbs[k].present && !lay.carbScrews[k].empty()) { last = lay.carbScrews[k].back().at; row = 0; found = true; }
        }
        if (!found && !lay.bodyScrews.empty()) { last = lay.bodyScrews.back().at; row = manifoldClass == 8 ? 2 : 1; }
        const int top = last.y - 1;
        if (sr2MachineDir == 0 && sr2MachineX == 8) zones.emplace_back("tune_a", "A tune bolt", SDL_Rect{last.x + kTuneOff[row][0] - 3, top + kTuneOff[row][1] - 3, 10, 10});
        if (sr2MachineDir == 0 && sr2MachineX == 8) zones.emplace_back("tune_b", "A tune bolt", SDL_Rect{last.x + kTuneOff[row][2] - 3, top + kTuneOff[row][3] - 3, 10, 10});
        // 76 columns of the machine; the 80th holds the red notch, 14 + (tune + 128) / 16 across. While it slides
        // (FUN_002562a2) the right-hand columns appear first at x 12, then the whole machine moves on to x 16.
        Bitmap* machine = Sr2Asset(SR2_ENGINE_TUNING_MACHINE);
        const int x = sr2MachineX;
        const int w = x + 72;
        if (x >= -73 && w > 0) {
            const int sx = w < 76 ? 76 - w : 0;
            const int width = w < 76 ? w : 76;
            const int dx = w < 76 ? 12 : x + 8;
            cfg.bitmaps["machine"] = {machine, {dx, 63, width, 38}, 20, {sx, 0, width, 38}};
            const int step = (car->GetSr2Tune() + 128) / 16 + 14;
            if (sx <= step) {
                const int nx = sx < 1 ? step + dx : step - sx + 12;
                cfg.bitmaps["notch"] = {machine, {nx, 79, 1, 9}, 21, {79, 16, 1, 9}};
            }
        }
    }
    auto addScrewZone = [&](int index, SDL_Point anchor) {
        zones.emplace_back("screw_" + std::to_string(index), "A bolt", SDL_Rect{anchor.x - 2, anchor.y - 6, 8, 8});
    };

    if (engineIn) {
        addBitmap("engine", lay.engine.asset, lay.engine.at, 2);
        addBitmap("plug", sr2Wire ? 11 : 10, lay.wire, 3);
        addBitmap("loose_wire", 7, lay.looseWire, 6);
        zones.emplace_back("wire", "Wires", SDL_Rect{lay.wire.x, lay.wire.y, 24, 17});
    }
    if (manifoldIn) {
        addBitmap("manifold", lay.manifold.asset, lay.manifold.at, 4);
        const int bodyScrews = static_cast<int>(lay.bodyScrews.size());
        for (int i = 0; i < bodyScrews; ++i) {
            const SDL_Point anchor = lay.bodyScrews[i].at;
            if (sr2Screw[i] < 2 || sr2Phase[kManifold] == PartInstallPhase::SCREWING_IN) {
                const int h = BoltHeight(sr2Screw[i]);
                addBitmap("screw_" + std::to_string(i), 6, {anchor.x, anchor.y - h + 1}, 7);
                DialogBitmap& db = cfg.bitmaps["screw_" + std::to_string(i)];
                db.rect = {anchor.x, anchor.y - h + 1, kScrewW, h};
                db.sourceRect = {0, 0, kScrewW, h};
            }
            addScrewZone(i, anchor);
        }
    }
    for (int k = 0; k < slots && k < 3; ++k) {
        if (sr2Phase[kCarb0 + k] == PartInstallPhase::EMPTY || !lay.carbs[k].present) continue;
        addBitmap("carb_" + std::to_string(k), lay.carbs[k].asset, lay.carbs[k].at, 5 + k);
        if (lay.airFilters[k].present) addBitmap("filter_" + std::to_string(k), lay.airFilters[k].asset, lay.airFilters[k].at, 8);
        for (int j = 0; j < 2 && j < static_cast<int>(lay.carbScrews[k].size()); ++j) {
            const int index = Sr2BayScrewFirst(kCarb0 + k) + j;
            const SDL_Point anchor = lay.carbScrews[k][j].at;
            if (sr2Screw[index] < 2 || sr2Phase[kCarb0 + k] == PartInstallPhase::SCREWING_IN) {
                const int h = BoltHeight(sr2Screw[index]);
                addBitmap("screw_" + std::to_string(index), 6, {anchor.x, anchor.y - h + 1}, 9);
                DialogBitmap& db = cfg.bitmaps["screw_" + std::to_string(index)];
                db.rect = {anchor.x, anchor.y - h + 1, kScrewW, h};
                db.sourceRect = {0, 0, kScrewW, h};
            }
            addScrewZone(index, anchor);
        }
    }

    // Where an empty slot's part goes, so clicking the gap opens the spare list.
    auto addPartZone = [&](int slot, const char* name, SDL_Point at, int asset) {
        if (sr2Phase[slot] != PartInstallPhase::EMPTY) return;
        const SDL_Rect nat = Sr2Asset(asset)->GetNaturalSize();
        zones.emplace_back("part_" + std::to_string(slot), name, SDL_Rect{at.x, at.y, nat.w, nat.h});
    };
    for (int k = slots - 1; k >= 0; --k) {
        if (lay.carbs[k].present) addPartZone(kCarb0 + k, "Carburetor", lay.carbs[k].at, lay.carbs[k].asset);
    }
    addPartZone(kManifold, "Manifold", lay.manifold.at, lay.manifold.asset);
    addPartZone(kEngine, "Engine", lay.engine.at, lay.engine.asset);
    cfg.zones = zones;

    auto invisible = [](SDL_Rect r, const char* text, int index) {
        ButtonConfig b;
        b.bounds = r;
        b.text = text;
        b.index = index;
        b.invisible = true;
        return b;
    };
    cfg.buttons = {invisible(kTuneRect, "Tune", 0), invisible(kPartsRect, "Parts", 1), invisible(kDoneRect, "Done", 2)};
    cfg.buttons[2].cancelChoice = true;
    cfg.onButtonPressed = [this](int index) {
        switch (index) {
            case 0:
                if (sr2Tuning) break;
                if (!Sr2BayReadyToTune()) { BayHint(GameText::Msg(GameText::CANT_TUNE)); break; }
                sr2Tuning = true;
                sr2MachineX = -76;
                sr2MachineDir = 1;
                sr2MachineAcc = 0.0;
                sr2BayDirty = true;
                break;
            case 1: if (!sr2Tuning) Sr2BayPartsClicked(); break;
            case 2:
                if (sr2Tuning) {
                    if (sr2MachineDir == 0) {
                        sr2MachineDir = -1;
                        sr2MachineAcc = 0.0;
                    }
                    break;
                }
                sr2BayDialog->Hide();
                SetActiveDialog(nullptr);
                break;
            default: break;
        }
    };

    const bool wasActive = activeDialog == sr2BayDialog.get() && sr2BayDialog;
    sr2BayDialog = std::make_unique<Dialog>(cfg);
    if (wasActive) {
        activeDialog = sr2BayDialog.get();
        sr2BayDialog->Show();
    }
}

void Garage::Sr2BayZoneClicked(const std::string& id) {
    if (id.rfind("screw_", 0) == 0) { Sr2BayScrewClicked(std::stoi(id.substr(6))); return; }
    if (id == "tune_a" || id == "tune_b") {
        Car* car = state.GetCurrentCar();
        if (sr2MachineDir != 0) return;
        if (car) car->SetSr2Tune((car->GetSr2Tune() + 128 + (id == "tune_a" ? 6 : 37)) % 256 - 128);
        sr2BayDirty = true;
        return;
    }
    if (sr2Tuning) return;
    if (id == "wire") { Sr2BayWireClicked(); return; }
    // The original has no clickable gaps; this is Parts under another name, so it opens the next slot that needs a part.
    if (id.rfind("part_", 0) == 0) Sr2BayPartsClicked();
}

// The only bay sound the emulator plays: the ratchet, 0x22c228, `rnd%20 + 10 + 130` at volume 0x2f. The original
// layers a second voice 3 periods higher on the other channel.
void Garage::Sr2BayRatchet() {
    if (!audioManager) return;
    const int period = 140 + std::rand() % 20;
    audioManager->PlaySound(AudioSampleType::HISS, 0x2f / 64.0f, 3579545 / period);
}

void Garage::Sr2BayScrewClicked(int screw) {
    const int slot = screw < 6 ? kManifold : kCarb0 + (screw - 6) / 2;
    PartInstallPhase& phase = sr2Phase[slot];
    if (phase == PartInstallPhase::MOUNTED) {
        if (!Sr2BayIsTop(slot)) { BayHint("Take the top part off first`!"); return; }
        phase = PartInstallPhase::UNSCREWING;
    }
    if (phase == PartInstallPhase::UNSCREWING) {
        if (sr2Screw[screw] < 2) sr2Screw[screw]++;
        Sr2BayRatchet();
        if (Sr2BaySlotOut(slot)) Sr2BayEmptySlot(slot);
    } else if (phase == PartInstallPhase::SCREWING_IN) {
        if (sr2Screw[screw] > 0) sr2Screw[screw]--;
        Sr2BayRatchet();
        if (Sr2BaySlotSeated(slot)) Sr2BayCommit(slot);
    }
    sr2BayDirty = true;
}

void Garage::Sr2BayWireClicked() {
    PartInstallPhase& phase = sr2Phase[kEngine];
    if (phase == PartInstallPhase::MOUNTED) {
        if (!Sr2BayIsTop(kEngine)) { BayHint("Take the top part off first`!"); return; }
        phase = PartInstallPhase::UNSCREWING;
    }
    if (phase != PartInstallPhase::UNSCREWING && phase != PartInstallPhase::SCREWING_IN) return;
    sr2Wire = !sr2Wire;
    Sr2BayRatchet();
    if (phase == PartInstallPhase::UNSCREWING && Sr2BaySlotOut(kEngine)) Sr2BayEmptySlot(kEngine);
    else if (phase == PartInstallPhase::SCREWING_IN && Sr2BaySlotSeated(kEngine)) Sr2BayCommit(kEngine);
    sr2BayDirty = true;
}

// The part goes to spares and the car stops running until something is fitted.
void Garage::Sr2BayEmptySlot(int slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (slot >= kCarb0) {
        const int idx = slot - kCarb0;
        if (car->HasCarburetor(idx)) {
            state.AddSpareCarburetor(car->GetCarburetor(idx));
            car->RemoveCarburetor(idx);
        }
    } else if (slot == kEngine) {
        if (car->HasEngine()) {
            EnginePart old;
            if (car->GetSpecEngineClass() >= 0) {
                const CarEngine_t type = car->GetEngineType();
                old = EnginePart(type, 0, 0, car->GetModelRef().vendor, static_cast<AmigaLibAsset>(Sr2EngineAsset(static_cast<int>(type) % 3)));
            } else {
                old = car->GetEngine();
            }
            state.AddSpareEngine(old);
            car->RemoveEngine();
        }
    } else if (car->HasManifold()) {
        Part old = car->GetSpecManifoldClass() >= 0
            ? CarBuilder::CreateManifoldPart(car->GetManifoldType(), car->GetModelRef().vendor)
            : car->GetManifold();
        state.AddSpareManifold(old);
        car->RemoveManifold();
    }
    sr2Phase[slot] = PartInstallPhase::EMPTY;
    const int first = Sr2BayScrewFirst(slot);
    for (int i = first; i < first + Sr2BayScrewCount(slot) && i < kSr2BayScrews; ++i) sr2Screw[i] = 2;
}

void Garage::Sr2BayCommit(int slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (slot == kEngine) {
        state.RemoveSpareEngine(sr2PendingEngine.engineType);
        car->SetEngine(sr2PendingEngine);
    } else if (slot == kManifold) {
        state.RemoveSpareManifold(sr2PendingManifold.partType.manifoldType);
        car->SetManifold(sr2PendingManifold);
        for (int i = 0; i < kMaxCarburetors; ++i) car->RemoveCarburetor(i);
    } else {
        const int idx = slot - kCarb0;
        state.RemoveSpareCarburetor(sr2PendingCarb[idx].partType.carburetorType);
        car->SetCarburetor(idx, sr2PendingCarb[idx]);
    }
    sr2Phase[slot] = PartInstallPhase::MOUNTED;
}

void Garage::Sr2BayPartsClicked() {
    for (int slot = 0; slot < kSr2BaySlots; ++slot) {
        if (sr2Phase[slot] != PartInstallPhase::EMPTY) continue;
        Car* car = state.GetCurrentCar();
        if (slot >= kCarb0 && car && slot - kCarb0 >= CarburetorsPerManifold(sr2Phase[kManifold] == PartInstallPhase::SCREWING_IN
                                                                                  ? sr2PendingManifold.partType.manifoldType
                                                                                  : car->GetManifoldType())) {
            continue;
        }
        if (slot > kEngine && sr2Phase[slot - 1] != PartInstallPhase::MOUNTED) {
            bubbles[8]->SetVisible(true);   // NO_ROOM_FOR_PART
            return;
        }
        Sr2BayShowSpares(slot);
        return;
    }
    bubbles[8]->SetVisible(true);
}

void Garage::BayHint(const std::string& text) {
    if (!sr2HintBubble) {
        auto bubble = std::make_unique<Bubble>(text, BUBBLE_LEFT, true);
        sr2HintBubble = bubble.get();
        bubbles.push_back(std::move(bubble));
    }
    sr2HintBubble->SetText(text);
    sr2HintBubble->SetVisible(true);
}

void Garage::Sr2BayShowSpares(int slot) {
    std::vector<std::string> labels;
    if (slot == kEngine) {
        if (state.GetSpareEngines().empty()) { bubbles[9]->SetVisible(true); return; }
        for (const EnginePart& p : state.GetSpareEngines()) labels.push_back(SpareEngineName(p) + " " + WearNote(p.wear));
    } else if (slot == kManifold) {
        if (state.GetSpareManifolds().empty()) { bubbles[10]->SetVisible(true); return; }
        for (const Part& p : state.GetSpareManifolds()) labels.push_back(SpareManifoldName(p) + " " + WearNote(p.wear));
    } else {
        if (state.GetSpareCarburetors().empty()) { bubbles[11]->SetVisible(true); return; }
        for (const Part& p : state.GetSpareCarburetors()) labels.push_back(SpareCarbName(p) + " " + WearNote(p.wear));
    }

    SDL_Rect bounds = {20, 17, 245, 136};
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect changeRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetRect = {160, 127, buttonWidth, buttonHeight};
    SDL_Rect scrollTopRect = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0, 208, 0, 255};
    cfg.borderColor = {0, 111, 239, 255};
    cfg.buttons = {{changeRect, "Change", nullptr, true, 0}, {forgetRect, "Forget it", nullptr, false, 1}};
    cfg.buttons[1].cancelChoice = true;
    cfg.lines = {{bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1}};
    cfg.zones = {Zone("spares_change", "Change", changeRect), Zone("spares_forget", "Forget it", forgetRect),
                 Zone("spares_top", std::string(1, MAIN_FONT_SCROLL_TOP), scrollTopRect),
                 Zone("spares_down", std::string(1, MAIN_FONT_SCROLL_DOWN), scrollDownRect),
                 Zone("spares_up", std::string(1, MAIN_FONT_SCROLL_UP), scrollUpRect),
                 Zone("spares_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect)};
    cfg.contentAreas = {topRect, bottomRect};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = {DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10},
                                  slot == kEngine ? "Spare engines" : slot == kManifold ? "Spare manifolds" : "Spare carburetors",
                                  {0xFF, 0xFF, 0xFF, 0xFF}, true}};
    cfg.onZoneHover = [this](Zone& zone) { hud->SetText(zone.GetText().c_str()); };
    cfg.onButtonPressed = [this, slot](int index) {
        const int selected = sr2BayList ? sr2BayList->GetSelectedIndex() : -1;
        sr2BayList->Hide();
        // Before installing: switching dialogs hides every bubble, the won't-fit one included.
        SetActiveDialog(sr2BayDialog.get());
        if (index == 0 && selected >= 0) Sr2BayInstall(slot, selected);
    };

    sr2BayList = std::make_unique<ListDialog>(
        cfg, ListDialogConfig{labels, {}, [](int) {}, listBoxRect, selectedItemRect,
                              {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}});
    SetActiveDialog(static_cast<Dialog*>(sr2BayList.get()));
}

void Garage::Sr2BayInstall(int slot, int index) {
    const CarPtr& car = state.GetCurrentCarPtr();
    if (!car) return;
    const CarVendor_t make = car->GetModelRef().vendor;
    bool fits = false;
    if (slot == kEngine) {
        const auto& spares = state.GetSpareEngines();
        if (index >= static_cast<int>(spares.size())) return;
        sr2PendingEngine = spares[index];
        fits = PartsCompatibilityManager::IsVendorCompatible(sr2PendingEngine.vendor, make);
    } else if (slot == kManifold) {
        const auto& spares = state.GetSpareManifolds();
        if (index >= static_cast<int>(spares.size())) return;
        sr2PendingManifold = spares[index];
        fits = PartsCompatibilityManager::IsVendorCompatible(sr2PendingManifold.vendor, make) &&
               PartsCompatibilityManager::IsManifoldCompatible(sr2PendingManifold.partType.manifoldType, car->GetEngineType());
    } else {
        const auto& spares = state.GetSpareCarburetors();
        if (index >= static_cast<int>(spares.size())) return;
        sr2PendingCarb[slot - kCarb0] = spares[index];
        fits = PartsCompatibilityManager::IsVendorCompatible(sr2PendingCarb[slot - kCarb0].vendor, make) &&
               PartsCompatibilityManager::IsCarburetorCompatible(sr2PendingCarb[slot - kCarb0].partType.carburetorType,
                                                                  car->GetManifoldType());
    }
    // lhood.c 0x256718: the engine, manifold or carburetor takes 900 / 450 / 225 ticks, a misfit 1200 / 600 / 300.
    const uint32_t ticks = slot == kEngine ? 900 : slot == kManifold ? 450 : 225;
    state.AdvanceTicks(fits ? ticks : ticks * 4 / 3);
    if (!fits) {
        bubbles[12]->SetVisible(true);
        return;
    }
    sr2Phase[slot] = PartInstallPhase::SCREWING_IN;
    const int first = Sr2BayScrewFirst(slot);
    for (int i = first; i < first + Sr2BayScrewCount(slot) && i < kSr2BayScrews; ++i) sr2Screw[i] = 2;
    if (slot == kEngine) sr2Wire = false;
    sr2BayDirty = true;
}

// One step per 1/50 s, as the original waits for the next frame between draws.
void Garage::UpdateSr2MachineAnimation(double deltaTime) {
    if (!sr2Tuning || sr2MachineDir == 0) return;
    sr2MachineAcc += deltaTime;
    bool moved = false;
    while (sr2MachineAcc >= 1.0 / 50.0 && sr2MachineDir != 0) {
        sr2MachineAcc -= 1.0 / 50.0;
        sr2MachineX += sr2MachineDir;
        moved = true;
        if (sr2MachineDir > 0 && sr2MachineX >= 8) {
            sr2MachineX = 8;
            sr2MachineDir = 0;
        } else if (sr2MachineDir < 0 && sr2MachineX <= -77) {
            sr2MachineX = 8;
            sr2MachineDir = 0;
            sr2Tuning = false;
        }
    }
    if (moved) sr2BayDirty = true;
}
