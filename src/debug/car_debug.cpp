#include "debug/car_debug.h"

#include "core/log.h"
#include "core/state.h"
#include "car/car_set.h"
#include "race/driver.h"
#include "gfx/SFont.h"

namespace {
constexpr int kCarX = 30;
constexpr int kCarY = 55;
constexpr SDL_Rect kPrevRect = {8,   170, 26, 16};
constexpr SDL_Rect kNextRect = {38,  170, 26, 16};
constexpr SDL_Rect kDumpRect = {250, 170, 62, 16};
constexpr uint32_t kWhite = 0xFFFFFFFF;
}

CarDebug::CarDebug() : Scene() {}

void CarDebug::Init(bool debugMode) {
    Scene::Init(debugMode);

    LoadModels();

    debugDriver.name = "Debug";
    debugDriver.spriteIndex = DRIVER_FIRST + portraitIndex;

    ButtonConfig prev{};
    prev.bounds = kPrevRect;
    prev.text = "<";
    prev.onClick = [this]() { Cycle(-1); };
    prevButton = std::make_unique<Button>(prev);

    ButtonConfig next{};
    next.bounds = kNextRect;
    next.text = ">";
    next.onClick = [this]() { Cycle(1); };
    nextButton = std::make_unique<Button>(next);

    ButtonConfig dump{};
    dump.bounds = kDumpRect;
    dump.text = "Dump";
    dump.onClick = [this]() { DumpAdjustments(); };
    dumpButton = std::make_unique<Button>(dump);

    SFont_Font* font = state.GetAssets().GetMainFont();
    titleLabel.Init(font, 8, 120, kWhite);
    spriteLabel.Init(font, 8, 132, kWhite);
    wheelLabel.Init(font, 8, 144, kWhite);
    driverLabel.Init(font, 8, 156, kWhite);

    ShowModel(0);
}

void CarDebug::RefreshLabels() {
    if (models.empty()) {
        titleLabel.SetText("no car models loaded");
        return;
    }
    const CarModel& model = models[currentIndex];
    char line[160];

    snprintf(line, sizeof(line), "%d/%d  %s",
             currentIndex + 1, static_cast<int>(models.size()), model.name.c_str());
    titleLabel.SetText(line);

    snprintf(line, sizeof(line), "set %s  index %d  body %d  %dx%d  C roof %s",
             GetCarSetDefs()[model.carSet].id.c_str(), model.index,
             model.sprites.body, model.width, model.height,
             model.sprites.choppedRoof < 0 ? "-" : roofChopped ? "chopped" : "stock");
    spriteLabel.SetText(line);

    snprintf(line, sizeof(line), "wheels Y %d (stock %d %+d)   up/down  0 reset  D dump",
             StockWheelY() + CurrentAdjust(), StockWheelY(), CurrentAdjust());
    wheelLabel.SetText(line);

    const SDL_Point at = carBitmap ? carBitmap->GetDriverOffsetFromBody() : SDL_Point{0, 0};
    snprintf(line, sizeof(line), "driver %d,%d (data %d,%d)  portrait %d   IJKL  P next",
             at.x, at.y, model.offsets.offsetXDriver, model.offsets.offsetYDriver,
             portraitIndex);
    driverLabel.SetText(line);
}

namespace {
BitmapOffset_t ReadSpriteOffsets(const byte_vector& data, size_t tableStart, int carIndex) {
    BitmapOffset_t offsets = {};
    const size_t o = tableStart + static_cast<size_t>(carIndex) * 12;
    if (o + 12 > data.size()) return offsets;
    const uint8_t* p = data.data() + o;
    offsets.offsetXRearTire    = p[0];
    offsets.offsetXFrontTire   = p[1];
    offsets.offsetXDriver      = p[2];
    offsets.offsetYDriver      = p[3];
    offsets.offsetXChoppedRoof = p[4];
    offsets.offsetXAirScoop    = p[5];
    offsets.offsetYAirScoop    = p[6];
    offsets.offsetYRearBumper  = p[7];
    offsets.offsetXFrontBumper = p[8];
    offsets.offsetYFrontBumper = p[9];
    offsets.offsetXDecal       = p[10];
    offsets.offsetYDecal       = p[11];
    return offsets;
}
}

void CarDebug::LoadModels() {
    models.clear();
    for (const CarModel& model : state.carData.GetAllModels()) {
        models.push_back(model);
    }
    AppendKingModels();
    LOG_INFO("CarDebug: %zu models across %zu car sets",
             models.size(), GetCarSetDefs().size());
}

void CarDebug::AppendKingModels() {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    const int kingSlot = SR_DATA_CARSPEC_ENTRY_COUNT - 1;

    for (int set = 0; set < static_cast<int>(defs.size()); set++) {
        CarModel model;
        model.carSet = set;
        model.name = "The King's car (" + defs[set].id + ")";

        if (defs[set].art == CarSetArt::AmigaLibP) {
            model.sprites.body = THE_KING_CHEVROLET_CORVETTE_1963_BODY;
            model.index = model.sprites.body;
            model.offsets = ReadSpriteOffsets(state.GetAssets().GetSrData(),
                                              SR_DATA_OFFSETS_START, kingSlot);
            const auto& entries = state.GetAssets().GetImageEntries();
            if (model.sprites.body >= entries.first) continue;
            model.width = entries.second[model.sprites.body].width;
            model.height = entries.second[model.sprites.body].height;
        } else {
            if (!state.GetAssets().HasCarSetData(set) || !state.GetAssets().IsCarSetEnabled(set)) {
                continue;
            }
            const byte_vector& data = state.GetAssets().GetCarSetData(set);
            DataParser parser;
            const std::vector<CarSpriteRefs> sprites = parser.ParseCarSpritesDOS(data);
            if (kingSlot >= static_cast<int>(sprites.size()) || sprites[kingSlot].body < 0) {
                continue;
            }
            model.sprites.body = sprites[kingSlot].body;
            model.index = set * HOT_DATA_SPRITE_BASE + model.sprites.body;
            model.offsets = ReadSpriteOffsets(data, HOT_DATA_OFFSETS_START, kingSlot);
            const auto& entries = state.GetAssets().GetCarSetImageEntries(set);
            if (model.sprites.body >= entries.first) continue;
            model.width = entries.second[model.sprites.body].width;
            model.height = entries.second[model.sprites.body].height;
        }

        // The King's sprite-table row is all zeros: no chop, no strippable bumpers.
        model.offsets.offsetYTires = CarSetWheelY(set, model.height);
        model.baseSpeed = 100;
        models.push_back(model);
    }
}

int CarDebug::CurrentAdjust() const {
    if (models.empty()) return 0;
    auto it = wheelYAdjust.find(models[currentIndex].index);
    return it == wheelYAdjust.end() ? 0 : it->second;
}

SDL_Point CarDebug::CurrentDriverAdjust() const {
    if (models.empty()) return {0, 0};
    auto it = driverAdjust.find(models[currentIndex].index);
    return it == driverAdjust.end() ? SDL_Point{0, 0} : it->second;
}

int CarDebug::StockWheelY() const {
    if (models.empty()) return 0;
    return models[currentIndex].offsets.offsetYTires;
}

void CarDebug::ShowModel(int index) {
    if (models.empty()) return;
    currentIndex = index;

    car = std::make_shared<Car>(models[currentIndex]);
    car->SetTires(STOCK);
    car->SetRoofChopped(roofChopped && models[currentIndex].sprites.choppedRoof >= 0);
    carBitmap = std::make_unique<CarBitmap>(car, &debugDriver, CarColorRed);
    carBitmap->Move({kCarX, kCarY});
    carBitmap->MoveWheelsYOffset(CurrentAdjust());
    const SDL_Point driverNudge = CurrentDriverAdjust();
    carBitmap->MoveDriverOffset(driverNudge.x, driverNudge.y);
    RefreshLabels();
}

void CarDebug::Cycle(int delta) {
    if (models.empty()) return;
    const int count = static_cast<int>(models.size());
    ShowModel(((currentIndex + delta) % count + count) % count);

    const CarModel& model = models[currentIndex];
    LOG_INFO("CarDebug: [%d/%d] %s (set '%s', index %d) offsetYTires=%d%+d",
             currentIndex + 1, count, model.name.c_str(),
             GetCarSetDefs()[model.carSet].id.c_str(), model.index,
             StockWheelY(), CurrentAdjust());
}

void CarDebug::AdjustWheelY(int delta) {
    if (models.empty() || !carBitmap) return;
    const CarModel& model = models[currentIndex];
    const int adjust = CurrentAdjust() + delta;
    wheelYAdjust[model.index] = adjust;
    carBitmap->MoveWheelsYOffset(adjust);
    RefreshLabels();

    LOG_INFO("CarDebug: %s (set '%s', index %d) offsetYTires %d -> %d (%+d)",
             model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(),
             model.index, StockWheelY(), StockWheelY() + adjust, adjust);
}

void CarDebug::AdjustDriver(int dx, int dy) {
    if (models.empty() || !carBitmap) return;
    const CarModel& model = models[currentIndex];
    SDL_Point nudge = CurrentDriverAdjust();
    nudge.x += dx;
    nudge.y += dy;
    driverAdjust[model.index] = nudge;
    carBitmap->MoveDriverOffset(dx, dy);
    RefreshLabels();

    const SDL_Point at = carBitmap->GetDriverOffsetFromBody();
    LOG_INFO("CarDebug: %s (set '%s', index %d) driver x=%d y=%d (data x_driver=%d y_driver=%d)",
             model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(), model.index,
             at.x, at.y, model.offsets.offsetXDriver, model.offsets.offsetYDriver);
}

void CarDebug::CyclePortrait() {
    portraitIndex = (portraitIndex + 1) % DRIVER_COUNT;
    debugDriver.spriteIndex = DRIVER_FIRST + portraitIndex;
    ShowModel(currentIndex);
    LOG_INFO("CarDebug: driver portrait %d (asset %d)", portraitIndex, debugDriver.spriteIndex);
}

void CarDebug::ToggleRoof() {
    roofChopped = !roofChopped;
    ShowModel(currentIndex);
    LOG_INFO("CarDebug: roof %s", roofChopped ? "chopped" : "stock");
}

void CarDebug::ResetModel() {
    if (models.empty() || !carBitmap) return;
    const CarModel& model = models[currentIndex];
    wheelYAdjust.erase(model.index);
    driverAdjust.erase(model.index);
    ShowModel(currentIndex);
    LOG_INFO("CarDebug: %s reset to offsetYTires=%d, driver x=%d y=%d",
             model.name.c_str(), StockWheelY(),
             carBitmap->GetDriverOffsetFromBody().x, carBitmap->GetDriverOffsetFromBody().y);
}

void CarDebug::DumpAdjustments() const {
    if (wheelYAdjust.empty()) {
        LOG_INFO("CarDebug: no wheel offsets touched yet");
        return;
    }

    // Zeroed after a nudge means checked and correct, so list it separately from unvisited ones.
    int changed = 0;
    for (const auto& entry : wheelYAdjust) {
        if (entry.second != 0) changed++;
    }
    LOG_INFO("CarDebug: %d corrected, %zu confirmed unchanged, %zu of %zu models not visited",
             changed, wheelYAdjust.size() - changed,
             models.size() - wheelYAdjust.size(), models.size());

    if (!driverAdjust.empty()) {
        LOG_INFO("CarDebug: driver placement, relative to the body top-left");
        for (const CarModel& model : models) {
            auto it = driverAdjust.find(model.index);
            if (it == driverAdjust.end() || (it->second.x == 0 && it->second.y == 0)) continue;
            LOG_INFO("  %-38s set=%-6s index=%4d  nudged x %+d y %+d  (data %d,%d)",
                     model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(), model.index,
                     it->second.x, it->second.y,
                     model.offsets.offsetXDriver, model.offsets.offsetYDriver);
        }
    }

    for (const CarModel& model : models) {
        auto it = wheelYAdjust.find(model.index);
        if (it == wheelYAdjust.end() || it->second == 0) continue;
        LOG_INFO("  %-38s set=%-6s index=%4d  %d -> %d",
                 model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(),
                 model.index, model.offsets.offsetYTires,
                 model.offsets.offsetYTires + it->second);
    }

    LOG_INFO("CarDebug: confirmed correct as-is");
    for (const CarModel& model : models) {
        auto it = wheelYAdjust.find(model.index);
        if (it == wheelYAdjust.end() || it->second != 0) continue;
        LOG_INFO("  %-38s set=%-6s index=%4d  %d",
                 model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(),
                 model.index, model.offsets.offsetYTires);
    }

    LOG_INFO("CarDebug: not visited");
    for (const CarModel& model : models) {
        if (wheelYAdjust.count(model.index)) continue;
        LOG_INFO("  %-38s set=%-6s index=%4d  %d",
                 model.name.c_str(), GetCarSetDefs()[model.carSet].id.c_str(),
                 model.index, model.offsets.offsetYTires);
    }
}

void CarDebug::DrawScene() {
    Scene::DrawScene();
    if (carBitmap) carBitmap->Draw();

    titleLabel.Draw();
    spriteLabel.Draw();
    wheelLabel.Draw();
    driverLabel.Draw();

    prevButton->Draw();
    nextButton->Draw();
    dumpButton->Draw();
}

bool CarDebug::PushEvent(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:  Cycle(-1); return true;
            case SDLK_RIGHT: Cycle(1);  return true;
            // Up raises the wheels, which means a smaller Y.
            case SDLK_UP:    AdjustWheelY(-1); return true;
            case SDLK_DOWN:  AdjustWheelY(1);  return true;
            case SDLK_i:     AdjustDriver(0, -1); return true;
            case SDLK_k:     AdjustDriver(0, 1);  return true;
            case SDLK_j:     AdjustDriver(-1, 0); return true;
            case SDLK_l:     AdjustDriver(1, 0);  return true;
            case SDLK_p:     CyclePortrait(); return true;
            case SDLK_c:     ToggleRoof(); return true;
            case SDLK_0:     ResetModel(); return true;
            case SDLK_d:     DumpAdjustments(); return true;
            default: break;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
        prevButton->HandleEvent(event);
        nextButton->HandleEvent(event);
        dumpButton->HandleEvent(event);
        return true;
    }

    return Scene::PushEvent(event);
}

void CarDebug::UpdateDebugInfo() {
    Scene::UpdateDebugInfo();
    if (models.empty()) {
        AddDebugValue("models", "none");
        return;
    }
    const CarModel& model = models[currentIndex];
    AddDebugValue("model", model.name);
    AddDebugValue("car set", GetCarSetDefs()[model.carSet].id);
    AddDebugValue("index", std::to_string(model.index));
    AddDebugValue("body sprite", std::to_string(model.sprites.body));
    AddDebugValue("chopped roof", std::to_string(model.sprites.choppedRoof));
    AddDebugValue("rear bumper", std::to_string(model.sprites.rearBumperStripped));
    AddDebugValue("front bumper", std::to_string(model.sprites.frontBumperStripped));
    AddDebugValue("wheels Y", std::to_string(StockWheelY() + CurrentAdjust()));
    AddDebugValue("wheels Y stock", std::to_string(StockWheelY()));
    AddDebugValue("adjusted", std::to_string(wheelYAdjust.size()) + " models");
}
