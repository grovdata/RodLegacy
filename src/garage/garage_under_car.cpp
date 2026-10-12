#include "core/game_text.h"
#include "garage/garage.h"
#include "core/state.h"
#include "core/assets.h"
#include "ui/hud.h"
#include "ui/main_font.h"
#include "car/car_system.h"

// Street Rod 2's "Under the car", lhood.c FUN_00256d04. See
// RodLegacyResearch/docs/sr2_amiga/screens.md, "Under the car".

namespace {
const SDL_Point kWindowPos = {24, 21};
const SDL_Rect kDoneRect = {141, 135, 44, 16};

struct UnderCarSlot {
    PartCategory category;
    const char* key;
    const char* label;
    SDL_Rect hotspot;  // the part's sprite sits at its top left
    GameText::Message noSpare;
};

const UnderCarSlot kSlots[] = {
    {PartCategory::Differential, "differential", "Differential", {80, 63, 23, 18},
     GameText::NO_SPARE_DIFF},
    {PartCategory::Muffler, "muffler", "Muffler", {168, 88, 31, 10},
     GameText::NO_SPARE_MUFFLER},
    {PartCategory::ExhaustManifold, "exhaust", "Exhaust manifold", {208, 80, 63, 15},
     GameText::NO_SPARE_EXHAUST},
};

Bitmap* Sr2Bitmap(int id) {
    return state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, id, SrGame::SR2});
}

int FittedClass(const Car& car, PartCategory c) {
    if (c == PartCategory::Differential) return car.GetDifferential();
    if (c == PartCategory::Muffler) return car.GetMuffler();
    return car.GetExhaust();
}

bool SlotEmpty(const Car& car, PartCategory c) {
    return c == PartCategory::Muffler && car.IsMufflerRemoved();
}

int SpriteFor(const Car& car, PartCategory c) {
    if (SlotEmpty(car, c)) return SR2_UNDER_CAR_NO_MUFFLER;
    if (c == PartCategory::Differential) return SR2_UNDER_CAR_DIFFERENTIAL + car.GetDifferential();
    if (c == PartCategory::Muffler) return SR2_UNDER_CAR_MUFFLER + car.GetMuffler();
    return SR2_UNDER_CAR_EXHAUST + car.GetExhaust();
}

CarVendor_t CarMake(const Car& car) {
    const CarVendor_t v = car.GetModelRef().vendor;
    return v == CarVendorUnknown ? SrLibQueryGetCarVendor(car.GetModelRef().index) : v;
}

DialogBitmap SpriteEntry(const Car& car, const UnderCarSlot& s) {
    Bitmap* b = Sr2Bitmap(SpriteFor(car, s.category));
    const SDL_Rect natural = b ? b->GetPosition() : SDL_Rect{};
    return {b, {s.hotspot.x, s.hotspot.y, natural.w, natural.h}, 1};
}
}

void Garage::ShowUnderCarDialog() {
    Car* car = state.GetCurrentCar();
    Bitmap* window = Sr2Bitmap(SR2_UNDER_CAR_WINDOW);
    if (!car || !window) return;
    const SDL_Rect natural = window->GetPosition();
    const SDL_Rect bounds = {kWindowPos.x, kWindowPos.y, natural.w, natural.h};

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0, 0, 0, 255};
    cfg.borderColor = {0, 0, 0, 0};
    cfg.contentAreas = {bounds};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.buttons = {
        {.bounds = kDoneRect, .text = "Done", .onClick = nullptr,
         .defaultChoice = false, .index = 0, .invisible = true, .cancelChoice = true},
    };
    cfg.bitmaps["background"] = {window, bounds, 0};
    for (const UnderCarSlot& s : kSlots) {
        cfg.zones.emplace_back(s.key, s.label, s.hotspot);
        cfg.bitmaps[s.key] = SpriteEntry(*car, s);
    }
    cfg.onZoneHover = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onZoneClick = [this](Zone& zone) {
        for (const UnderCarSlot& s : kSlots) {
            if (zone.GetId() == s.key) ShowUnderCarPicker(s.category);
        }
    };
    cfg.onButtonPressed = [this](int) {
        underCarDialog->Hide();
        SetActiveDialog(nullptr);
        StartCreeperBack();
    };

    underCarDialog = std::make_unique<Dialog>(cfg);
    SetActiveDialog(underCarDialog.get());
}

void Garage::RefreshUnderCarParts() {
    Car* car = state.GetCurrentCar();
    if (!car || !underCarDialog) return;
    for (const UnderCarSlot& s : kSlots) {
        if (DialogBitmap* db = underCarDialog->GetBitmap(s.key)) {
            *db = SpriteEntry(*car, s);
            if (db->bitmap) db->bitmap->SetVisible(true);
        }
    }
}

void Garage::ShowUnderCarPicker(PartCategory slot) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    underCarSlot = slot;
    underCarRemoveRow = slot == PartCategory::Muffler && !car->IsMufflerRemoved();

    std::vector<std::string> labels;
    if (underCarRemoveRow) labels.push_back("Remove current muffler");
    for (const Part& p : state.GetSpareSr2(slot)) {
        labels.push_back(p.name.empty() ? Sr2PartName(slot, p) : p.name);
    }
    if (labels.empty()) {
        for (const UnderCarSlot& s : kSlots) {
            if (s.category == slot) bubbles[kNoSpareSr2Bubble]->SetText(GameText::Msg(s.noSpare));
        }
        bubbles[kNoSpareSr2Bubble]->SetVisible(true);
        return;
    }

    SDL_Rect bounds = {20, 17, 245, 136};
    const int buttonWidth = 66;
    const int buttonHeight = 16;
    SDL_Rect topRect = {bounds.x + 1, bounds.y + 1, bounds.w - 14, 95};
    SDL_Rect bottomRect = {bounds.x + 1, topRect.y + topRect.h, bounds.w - 14, 40};
    SDL_Rect selectedItemRect = {bounds.x + 3, bottomRect.y + 4, bounds.w - 18, 12};
    SDL_Rect listBoxRect = {bounds.x + 4, bounds.y + 14, bounds.w - 18, 80};
    SDL_Rect changeRect = {59, 127, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {160, 127, buttonWidth, buttonHeight};
    SDL_Rect scrollTopRect    = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect     = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect   = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    std::vector<ButtonConfig> buttons = {
        {changeRect, "Change", nullptr, true, 0},
        {forgetItRect, "Forget it", nullptr, false, 1},
    };
    buttons[1].cancelChoice = true;

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0, 208, 0, 255};
    cfg.borderColor = {0, 111, 239, 255};
    cfg.buttons = buttons;
    cfg.texts = {DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10},
                                  "Spare parts", {0xFF, 0xFF, 0xFF, 0xFF}, true}};
    cfg.lines = {{bounds.x + 4, listBoxRect.y - 1, bounds.w - 6, 1}};
    cfg.zones = {
        Zone("under_change", "Change", changeRect),
        Zone("under_forget_it", "Forget it", forgetItRect),
        Zone("under_scroll_top", std::string(1, MAIN_FONT_SCROLL_TOP), scrollTopRect),
        Zone("under_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN), scrollDownRect),
        Zone("under_scroll_up", std::string(1, MAIN_FONT_SCROLL_UP), scrollUpRect),
        Zone("under_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect),
    };
    cfg.contentAreas = {topRect, bottomRect};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.onZoneHover = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onButtonPressed = [this](int index) {
        const int row = underCarPartsDialog->GetSelectedIndex();
        underCarPartsDialog->Hide();
        SetActiveDialog(underCarDialog.get());
        if (index == 0 && row >= 0) PickUnderCarPart(row);
    };

    underCarPartsDialog = std::make_unique<ListDialog>(
        cfg,
        ListDialogConfig{labels, {}, [](int) {}, listBoxRect, selectedItemRect,
                         {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}});
    SetActiveDialog(underCarPartsDialog.get());
}

void Garage::PickUnderCarPart(int row) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    std::vector<Part>& spares = state.GetSpareSr2(underCarSlot);
    const CarVendor_t make = CarMake(*car);
    const int value = car->GetValue();
    const bool wasEmpty = SlotEmpty(*car, underCarSlot);
    const Part old = MakeSr2Part(underCarSlot, FittedClass(*car, underCarSlot), make);

    if (underCarRemoveRow) {
        if (row == 0) {
            spares.push_back(old);
            car->SetMufflerRemoved(true);
            car->SetValue(value - old.cost);
            RefreshUnderCarParts();
            return;
        }
        row--;
    }
    if (row < 0 || row >= (int)spares.size()) return;
    const Part picked = spares[row];
    if (!PartsCompatibilityManager::IsVendorCompatible(picked.vendor, make)) {
        bubbles[12]->SetVisible(true);  // partIncompatibleBubble
        return;
    }

    spares.erase(spares.begin() + row);
    if (!wasEmpty) spares.push_back(old);
    int exhaust = car->GetExhaust();
    int muffler = car->GetMuffler();
    int differential = car->GetDifferential();
    if (underCarSlot == PartCategory::ExhaustManifold) exhaust = picked.partType.sr2Class;
    else if (underCarSlot == PartCategory::Muffler) muffler = picked.partType.sr2Class;
    else differential = picked.partType.sr2Class;
    car->SetSr2Parts(exhaust, muffler, differential);
    car->SetMufflerRemoved(false);
    car->SetValue(value - (wasEmpty ? 0 : old.cost) + picked.cost);
    RefreshUnderCarParts();
}

namespace {
// Two held frames, then one pixel right a frame; the climb is 28 rows over the 54 columns.
SDL_Point CreeperPos(int step) {
    const int x = 80 + (step > 1 ? step - 1 : 0);
    return {x, 172 - (x - 80) * 28 / 54};
}
}

void Garage::StartCreeperRoll() {
    creeperPhase = CreeperPhase::ROLLING_OUT;
    creeperStep = 0;
    creeperAcc = 0.0;
}

void Garage::StartCreeperBack() {
    if (creeperPhase == CreeperPhase::REST) return;
    creeperPhase = CreeperPhase::ROLLING_BACK;
    creeperStep = kCreeperLastStep;
    creeperAcc = 0.0;
}

void Garage::UpdateSr2Creeper(double deltaTime) {
    if (creeperPhase != CreeperPhase::ROLLING_OUT && creeperPhase != CreeperPhase::ROLLING_BACK) return;
    creeperAcc += deltaTime;
    while (creeperAcc >= 1.0 / 50.0 && (creeperPhase == CreeperPhase::ROLLING_OUT || creeperPhase == CreeperPhase::ROLLING_BACK)) {
        creeperAcc -= 1.0 / 50.0;
        if (creeperPhase == CreeperPhase::ROLLING_OUT) {
            if (++creeperStep >= kCreeperLastStep) {
                creeperStep = kCreeperLastStep;
                creeperPhase = CreeperPhase::PARKED;
                ShowUnderCarDialog();
            }
        } else if (--creeperStep <= 0) {
            creeperStep = 0;
            creeperPhase = CreeperPhase::REST;
        }
    }
}

// The jack and creeper lie on the floor; the creeper is drawn over the car while it rolls under it.
void Garage::DrawSr2Fixtures(bool front) {
    if (state.GetAssets().GetGame() != SrGame::SR2) return;
    if (!sr2Creeper) sr2Creeper = Sr2Bitmap(SR2_UNDER_CAR_CREEPER);
    if (!front) {
        if (jackAnim && tireChangePhase == TireChangePhase::IDLE) {
            jackAnim->DrawWithRects({32, 0, 32, 29}, {138, 157, 32, 29});
        }
        if (sr2Creeper && creeperPhase == CreeperPhase::REST) {
            sr2Creeper->SetPosition(80, 172);
            sr2Creeper->SetVisible(true);
            sr2Creeper->Draw();
        }
        return;
    }
    if (sr2Creeper && creeperPhase != CreeperPhase::REST) {
        const SDL_Point at = CreeperPos(creeperStep);
        sr2Creeper->SetPosition(at.x, at.y);
        sr2Creeper->SetVisible(true);
        sr2Creeper->Draw();
        if (jackAnim) {
            if (tireChangePhase == TireChangePhase::IDLE) {
                jackAnim->DrawWithRects({32, 0, 32, 29}, {138, 157, 32, 29});
            } else if (jackLastFrameRect.w > 0) {
                jackAnim->DrawWithRects(jackLastFrameRect, {138, 157, jackLastFrameRect.w, jackLastFrameRect.h});
            }
        }
    }
}
