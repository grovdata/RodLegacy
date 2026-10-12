#include "core/game_text.h"
#include "garage/garage.h"
#include "core/state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "core/assets.h"
#include "core/state.h"
#include "car/engine_bay.h"
#include "core/embedded_loader.h"
#include "embedded_resources.h"
#include "ui/main_font.h"
#include "audio/audio_manager.h"
#include "car/engine_part.h"
#include "parsers/libp.h"
#include "scenes/scene_layout.h"
#include "race/grudge.h"
#include "scenes/grudge_night.h"
#include <SDL_image.h>
#include <algorithm>

namespace {
bool IsSr2() { return state.GetAssets().GetGame() == SrGame::SR2; }

Asset GarageBackground() {
    if (IsSr2()) return (Asset){LibAssetRegular, SrLibFileAmigaLibP, SR2_GARAGE_SCREEN, SrGame::SR2};
    return (Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN};
}
}

Garage::Garage() : CarScene(GarageBackground()) {
    signRect = {72, 44, 195, 12};

    zones = SceneLayout::Load(IsSr2() ? embedded_garage_sr2_toml : embedded_garage_toml);

    if (Zone* drink = ZoneById("drink")) drink->onClick = [this] {
        if (!CanInteract()) return;
        bubbles[0]->SetVisible(true);
    };
    GetZone("newspaper").onClick = [this] {
        if (!CanInteract()) return;
        tripReturnTicks = 300;
        state.MoveToScene(SCENE_NEWSPAPER);
    };
    GetZone("tires").onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        if (state.GetSpareTires().empty()) bubbles[2]->SetVisible(true);
        else ShowTireDialog();
    };
    GetZone("paint_job").onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        ShowPaintDialog();
    };
    GetZone("gas_can").onClick = [this] {
        if (!CanInteract()) return;
        Car* car = state.GetCurrentCar();
        if (!car) { bubbles[7]->SetVisible(true); return; }
        // SR2's main loop 0x257ee8: full at 130.
        if (car->GetFuelTenths() >= (IsSr2() ? 130 : 140)) { bubbles[kFullTankBubble]->SetVisible(true); return; }
        if (state.GetBankroll() == 0) { bubbles[kGasNotFreeBubble]->SetVisible(true); return; }
        StartTrip();
        state.MoveToScene(SCENE_GAS_STATION);
    };
    GetZone("car_info").onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        ShowCarInfoDialog();
    };
    GetZone("calendar").onClick = [this] {
        if (!CanInteract()) return;
        ShowCalendarDialog();
    };
    GetZone("music").onClick = [this] {
        if (!CanInteract()) return;
        AudioManager* am = state.GetAssets().GetAudioManager();
        if (am) {
            bool nowMuted = !am->IsMusicMuted();
            am->SetMusicMuted(nowMuted);
            const std::string newText = nowMuted ? "Catch some tunes" : "Squelsh it !";
            SetZoneText("music", newText);
            hud->SetText(newText.c_str());
            state.GetAssets().SaveSettings();
        }
    };
    if (Zone* stickers = ZoneById("stickers")) stickers->onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        stickerScrollRow = 0;
        ShowStickerDialog();
    };
    GetZone("sell_parts").onClick = [this] {
        if (!CanInteract()) return;
        if (CollectSpares().empty()) bubbles[1]->SetVisible(true);
        else ShowSparePartsDialog();
    };
    GetZone("menu").onClick = [this] {
        if (!CanInteract()) return;
        SetActiveDialog((Dialog*)gameMenuDialog.get());
    };
    GetZone("select_car").onClick = [this] {
        if (!CanInteract()) return;
        sortedOwnedCars.clear();
        for (auto& car : state.GetOwnedCars()) sortedOwnedCars.push_back(car);
        std::sort(sortedOwnedCars.begin(), sortedOwnedCars.end(), [](const CarPtr& a, const CarPtr& b) {
            return a->GetModel().name < b->GetModel().name;
        });
        std::vector<std::string> labels;
        FreeCarListSwatches();
        for (const auto& car : sortedOwnedCars) {
            labels.push_back(car->GetModel().name);
            carListSwatches.push_back(CarBitmap::CreateColorSwatch(car->GetColor()));
        }
        carSelectDialog->SetLabels(labels, carListSwatches);
        SetActiveDialog((Dialog*)carSelectDialog.get());
    };
    GetZone("goto_diner").onClick = [this] {
        if (!CanInteract()) return;
        Car* car = state.GetCurrentCar();
        if (!car) { bubbles[7]->SetVisible(true); return; }
        if (!car->IsRunning()) { bubbles[13]->SetVisible(true); return; }
        if (car->GetFuelTenths() < 9) { bubbles[kNotEnoughGasBubble]->SetVisible(true); return; }
        StartTrip();
        state.MoveToScene(SCENE_DINER);
    };
    if (Zone* grudge = ZoneById("grudge_night")) grudge->onClick = [this] {
        if (!CanInteract()) return;
        Car* car = state.GetCurrentCar();
        if (!car) { bubbles[7]->SetVisible(true); return; }
        if (!srrace::GrudgeNightOpen(GameClock::DayNumber(state.GetElapsedTicks()))) {
            bubbles[kNotWednesdayBubble]->SetVisible(true);
            return;
        }
        if (!car->IsRunning()) { bubbles[13]->SetVisible(true); return; }
        if (car->GetFuelTenths() < srrace::GRUDGE_MIN_GAS) { bubbles[kNotEnoughGasBubble]->SetVisible(true); return; }
        if (state.GetBankroll() < srrace::GRUDGE_ENTRY_FEE) { bubbles[kNoGrudgeStakeBubble]->SetVisible(true); return; }
        state.GetGrudgeNight()->Begin();
        StartTrip();
        state.MoveToScene(SCENE_GRUDGE_NIGHT);
    };
    if (Zone* underCar = ZoneById("under_car")) underCar->onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        if (creeperPhase != CreeperPhase::REST) return;
        StartCreeperRoll();
    };
    GetZone("rear_bumper").onClick = [this] {
        if (!CanInteract()) return;
        ShowBumperDialog(false);
    };
    GetZone("front_bumper").onClick = [this] {
        if (!CanInteract()) return;
        ShowBumperDialog(true);
    };
    GetZone("roof").onClick = [this] {
        if (!CanInteract()) return;
        ShowRoofDialog();
    };
    GetZone("transmission").onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        // Opens without a spare too: you may just unscrew, or fit a new box after dropping one.
        ShowTransmissionInstallDialog();
    };
    GetZone("engine").onClick = [this] {
        if (!CanInteract()) return;
        if (IsSr2()) { OpenSr2EngineBay(); return; }
        engineBay->ResetToOverview();
        SetActiveDialog((Dialog*)engineBay->GetDialog());
    };

    auto bubbleDrink = std::make_unique<Bubble>(GameText::Msg(GameText::NO_DRINKING), BUBBLE_LEFT, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(std::move(bubbleDrink));
    auto partsBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_PARTS), BUBBLE_LEFT, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(std::move(partsBubble));
    auto tiresBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_TIRES), BUBBLE_LEFT, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(std::move(tiresBubble));
    auto paintBubble = std::make_unique<Bubble>(GameText::Msg(GameText::PAINT_NOT_FREE), BUBBLE_RIGHT, true);
    paintBubble->SetVisible(false);
    bubbles.push_back(std::move(paintBubble));
    auto customizationBubble = std::make_unique<Bubble>(GameText::Msg(GameText::CUSTOMIZATION_NOT_FREE), BUBBLE_RIGHT, true);
    customizationBubble->SetVisible(false);
    bubbles.push_back(std::move(customizationBubble));
    // The backtick renders as a space.
    auto roofConstraintBubble = std::make_unique<Bubble>(GameText::Msg(GameText::CANT_CHOP_ROOF), BUBBLE_LEFT, true);
    roofConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(roofConstraintBubble));
    auto bumperConstraintBubble = std::make_unique<Bubble>(GameText::Msg(GameText::BUMPER_GONE), BUBBLE_LEFT, true);
    bumperConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(bumperConstraintBubble));
    auto noCarBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_CAR), BUBBLE_LEFT, true);
    noCarBubble->SetVisible(false);
    bubbles.push_back(std::move(noCarBubble));
    // The backtick renders as a space (see roofConstraintBubble above).
    auto transNotEmptyBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_ROOM_FOR_PART), BUBBLE_RIGHT_TWO_LINES, true);
    transNotEmptyBubble->SetVisible(false);
    bubbles.push_back(std::move(transNotEmptyBubble));
    auto noSpareEngineBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_ENGINE), BUBBLE_LEFT, true);
    noSpareEngineBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareEngineBubble));
    auto noSpareManifoldBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_MANIFOLD), BUBBLE_LEFT, true);
    noSpareManifoldBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareManifoldBubble));
    auto noSpareCarbBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_CARB), BUBBLE_LEFT, true);
    noSpareCarbBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareCarbBubble));
    auto partIncompatibleBubble = std::make_unique<Bubble>(GameText::Msg(GameText::PART_WONT_FIT), BUBBLE_LEFT, true);
    partIncompatibleBubble->SetVisible(false);
    bubbles.push_back(std::move(partIncompatibleBubble));
    // bubbles[13] - shown when the player tries to leave with the engine apart.
    auto notRunningBubble = std::make_unique<Bubble>(GameText::Msg(GameText::CAR_NOT_RUNNING), BUBBLE_LEFT, true);
    notRunningBubble->SetVisible(false);
    bubbles.push_back(std::move(notRunningBubble));
    // Every buyer line, car or part; text set per message.
    auto haggleBubble = std::make_unique<Bubble>(GameText::Msg(GameText::HAGGLE_NO_THANKS), BUBBLE_RIGHT, true);
    haggleBubble->SetVisible(false);
    bubbles.push_back(std::move(haggleBubble));
    auto outOfDoughBubble = std::make_unique<Bubble>(
        GameText::Msg(GameText::OUT_OF_DOUGH),
        BUBBLE_RIGHT_TWO_LINES, false);
    outOfDoughBubble->SetVisible(false);
    bubbles.push_back(std::move(outOfDoughBubble));
    auto notEnoughGasBubble = std::make_unique<Bubble>(
        GameText::Msg(GameText::NOT_ENOUGH_GAS),
        BUBBLE_RIGHT_TWO_LINES, true);
    notEnoughGasBubble->SetVisible(false);
    bubbles.push_back(std::move(notEnoughGasBubble));
    auto fullTankBubble = std::make_unique<Bubble>(GameText::Msg(GameText::TANK_FULL), BUBBLE_RIGHT, true);
    fullTankBubble->SetVisible(false);
    bubbles.push_back(std::move(fullTankBubble));
    auto gasNotFreeBubble = std::make_unique<Bubble>(GameText::Msg(GameText::GAS_NOT_FREE), BUBBLE_RIGHT, true);
    gasNotFreeBubble->SetVisible(false);
    bubbles.push_back(std::move(gasNotFreeBubble));
    auto notWednesdayBubble = std::make_unique<Bubble>(GameText::Msg(GameText::NOT_WEDNESDAY), BUBBLE_RIGHT, true);
    notWednesdayBubble->SetVisible(false);
    bubbles.push_back(std::move(notWednesdayBubble));
    auto noGrudgeStakeBubble = std::make_unique<Bubble>(
        GameText::Msg(GameText::NO_GRUDGE_STAKE), BUBBLE_RIGHT, true);
    noGrudgeStakeBubble->SetVisible(false);
    bubbles.push_back(std::move(noGrudgeStakeBubble));
    auto noSpareSr2Bubble = std::make_unique<Bubble>(GameText::Msg(GameText::NO_SPARE_MUFFLER), BUBBLE_LEFT, true);
    noSpareSr2Bubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareSr2Bubble));

    currentPointer = SrLibAmigaSpriteAssetMousePointerWrench;

    signSurface = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(signSurface, SDL_BLENDMODE_BLEND);

    SDL_FillRect(signSurface, NULL, SDL_MapRGBA(signSurface->format, 0, 0, 0, 0));

    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);

    SetAudioManager(state.GetAssets().GetAudioManager());

    std::vector<std::string> labels;
    for (const auto& car : state.GetOwnedCars()) {
        labels.push_back(car->GetModel().name);
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
    SDL_Rect sellItRect = {bounds.x + 90, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {bounds.x + 165, buttonY, buttonWidth, buttonHeight};

    SDL_Rect scrollTopRect = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};     // top
    SDL_Rect scrollUpRect = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};     // up
    SDL_Rect scrollDownRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15}; // down
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};  // bottom

    Zone switchItZone("switch_it",    "Switch it", switchItRect);
    Zone sellItZone  ("sell_it",      "Sell it",   sellItRect);
    Zone forgetItZone("car_forget_it","Forget it", forgetItRect);
    Zone scrollTopZone   ("scroll_top",    std::string(1, MAIN_FONT_SCROLL_TOP),    scrollTopRect);
    Zone scrollDownZone  ("scroll_down",   std::string(1, MAIN_FONT_SCROLL_DOWN),   scrollDownRect);
    Zone scrollUpZone    ("scroll_up",     std::string(1, MAIN_FONT_SCROLL_UP),     scrollUpRect);
    Zone scrollBottomZone("scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {
            switchItRect,
            "Switch it",
            nullptr,
            true,
            0
        },
        {
            sellItRect,
            "Sell it",
            nullptr,
            false,
            1
        },
        {
            forgetItRect,
            "Forget it",
            nullptr,
            false,
            2
        }
    };
    buttons[2].cancelChoice = true;  // "Forget it"

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Your cars", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {40, 39, 240, 1},
    };

    std::vector<Zone> dialogZones = {
        switchItZone,
        sellItZone,
        forgetItZone,
        scrollTopZone,
        scrollDownZone,
        scrollUpZone,
        scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;
    dialogConfig.contentAreas = std::vector<SDL_Rect>{topRect, bottomRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        this->hud->SetText(zone.GetText().c_str());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Switch it
                int selectedIndex = carSelectDialog->GetSelectedIndex();
                if (selectedIndex >= 0 && selectedIndex < (int)sortedOwnedCars.size()) {
                    const CarPtr& selectedCar = sortedOwnedCars[selectedIndex];
                    if (selectedCar != state.GetCurrentCarPtr()) {
                        previewColor = selectedCar->GetColor();
                        originalColor = previewColor;
                        UpdatePaintCanPreview();
                        if (state.GetCurrentCar() == nullptr) {
                            // Nothing parked to back out first, so take one straight away or the switch leaves the player carless.
                            state.SetCurrentCar(selectedCar);
                            needsNewAnimation = true;
                            UpdateSign();
                            RefreshEngineBayGeometry();
                            StartAnimateCar(selectedCar, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
                        } else {
                            pendingCar = selectedCar;
                            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                        }
                    }
                }
                carSelectDialog->Hide();
                break;
            }
            case 1: { // Sell it
                int selectedIndex = carSelectDialog->GetSelectedIndex();
                carSelectDialog->Hide();
                if (selectedIndex >= 0 && selectedIndex < (int)sortedOwnedCars.size()) {
                    StartSellCar(sortedOwnedCars[selectedIndex]);
                } else {
                    SetActiveDialog(nullptr);
                }
                break;
            }
            case 2: // Forget it
                carSelectDialog->Hide();
                break;
        }
    };
    dialogConfig.texts = texts;

    ListDialogConfig listDialogConfig;
    listDialogConfig.labels = labels;
    listDialogConfig.listBoxRect = listBoxRect;
    listDialogConfig.selectedItemRect = selectedItemRect;

    carSelectDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            {},
            [this](int selectedIndex) {
                const CarPtr& selectedCar = sortedOwnedCars[selectedIndex];
            LOG_INFO("Garage::ListDialog: Selected index=%d, car=%s (ptr: %p)",
                   selectedIndex,
                   selectedCar->GetModel().name.c_str(),
                   (void*)selectedCar.get());

            previewColor = selectedCar->GetColor();
            originalColor = previewColor;
            LOG_INFO("Car list: Updating paint can to color %d for selected car", (int)previewColor);
            UpdatePaintCanPreview();

            if (selectedCar != state.GetCurrentCarPtr()) {
                pendingCar = selectedCar;
                carSelectDialog->Hide();
                StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                LOG_INFO("Garage::ListDialog: Switching from current car (ptr: %p) to pending car (ptr: %p)",
                       (void*)state.GetCurrentCar(),
                       (void*)pendingCar.get());
            } else {
                LOG_INFO("Garage::ListDialog: Same car selected, ignoring");
            }
        },
        listBoxRect,
        selectedItemRect,
        {
            scrollTopRect,
            scrollUpRect,
            scrollDownRect,
            scrollBottomRect
        }
    });

    StandardCarColor currentCarColor = state.GetCurrentCar() ? state.GetCurrentCar()->GetColor() : CarColorRed;
    previewColor = currentCarColor;
    originalColor = currentCarColor;
    LOG_INFO("Garage constructor: Initializing paint can to current car color %d", (int)currentCarColor);

    paintCanPreview = std::make_unique<Bitmap>(10, IsSr2() ? 9 : 12, 2);
    paintCanPreview->SetPosition(IsSr2() ? 208 : 233, IsSr2() ? 174 : 166);
    paintCanPreview->SetVisible(true);
    UpdatePaintCanPreview();

    InitGameMenuDialog();
    InitOptionsDialog();
    InitBumperDialogs();

    if (!IsSr2()) engineBay = std::make_unique<EngineBay>(
        state.GetCurrentCar()->GetTuneLevel(),
        state,
        [this](int buttonIndex) {
            switch (buttonIndex) {
                case 0: // Tune
                    if (engineBay->GetCurrentState() == ENGINE_BAY_OVERVIEW) {
                        // Nothing to tune while the engine is in pieces.
                        Car* car = state.GetCurrentCar();
                        if (!car || !car->IsRunning()) {
                            BayHint(GameText::Msg(GameText::CANT_TUNE));
                            break;
                        }
                        engineBay->SwitchToState(ENGINE_BAY_TUNING);
                        SetActiveDialog(engineBay->GetDialog());
                    } else {
                        engineBay->SwitchToState(ENGINE_BAY_OVERVIEW);
                        SetActiveDialog(engineBay->GetDialog());
                    }
                    break;
                case 1: // Parts
                    OnEngineBayPartsButtonClicked();
                    break;
                case 2: // Done
                    engineBay->GetDialog()->Hide();
                    SetActiveDialog(nullptr);
                    break;
                case 99: // Special case: just update dialog (used by tuning Done button)
                    SetActiveDialog(engineBay->GetDialog());
                    break;
            }
        },
        [this](Zone& zone) {
            this->hud->SetText(zone.GetText().c_str());
        },
        [this](Zone& zone) {
            OnEngineBayZoneClicked(zone);
        }
    );


    // Street Rod 2's jack sheet has the same frames: three poses and the floor patch.
    auto* jackBitmap = IsSr2() ? state.GetAssets().GetBitmapAsset(
                                     {LibAssetRegular, SrLibFileAmigaLibP, SR2_CAR_JACK, SrGame::SR2})
                               : state.GetAssets().GetBitmapAsset(
                                     {LibAssetRegular, SrLibFileAmigaLibP, CAR_JACK_ANIMATION});
    if (jackBitmap) {
        const SpriteAnimationInfo* animInfo = GetSpriteAnimationInfo(CAR_JACK_ANIMATION);
        if (animInfo && animInfo->frameCount > 0) {
            SDL_Rect frameRect = {0, 0, CAR_JACK_FRAMES[0].w, CAR_JACK_FRAMES[0].h};
            jackAnim = std::make_unique<AnimatedBitmap>(jackBitmap, frameRect);
            jackAnim->SetAnimationFromInfo(animInfo);
            jackAnim->SetLoop(false);
            jackTotalFrames = animInfo->frameCount;
            // The frame after the animation frames is the background mask.
            const SpriteRect& maskFrame = CAR_JACK_FRAMES[animInfo->frameCount];
            jackLastFrameRect = {maskFrame.x, maskFrame.y, maskFrame.w, maskFrame.h};
            LOG_INFO("Garage: Loaded jack animation: %d frames, mask frame at %d,%d %dx%d",
                     jackTotalFrames, maskFrame.x, maskFrame.y, maskFrame.w, maskFrame.h);
        }
    } else {
        LOG_INFO("Garage: Jack animation asset not found (CAR_JACK_ANIMATION=%d)", CAR_JACK_ANIMATION);
    }
}


SDL_Point Garage::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GARAGE);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, state.GetCurrentCar()->GetModel().height);
    return {x, y};
}

void Garage::UpdateSign() {

    LOG_INFO("Garage::UpdateSign");
    // The garage can be carless, and then the sign says nothing.
    Car* current = state.GetCurrentCar();
    char text[50];
    snprintf(text, 50, "%s", current ? current->GetModel().name.c_str() : "");

    Uint32 transparent = SDL_MapRGBA(signSurface->format, 0, 0, 0, 0);
    SDL_FillRect(signSurface, NULL, transparent);

    if (text[0] != '\0') {
        int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), text);
        int x = signRect.x + (signRect.w - textWidth) / 2;
        SFont_Write(signSurface, state.GetAssets().GetMainFont(), x, 45, text);
    }

    if (signTexture) {
        SDL_DestroyTexture(signTexture);
    }

    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
}


void Garage::DrawScene() {
    const bool dialogUp = GetActiveDialog() != nullptr && GetActiveDialog()->IsVisible();
    if (!outOfDough && !dialogUp && !animatingCar && !sparePartsNeedsReopen &&
        state.IsOutOfDough()) {
        outOfDough = true;
        bubbles[kOutOfDoughBubble]->SetVisible(true);
        LOG_INFO("Garage: out of dough - the run is over");
    }
    if (rebuildOptions_) {
        rebuildOptions_ = false;
        InitOptionsDialog();
        SetActiveDialog(optionsDialog.get());
    }
    if (rebuildSound_) {
        rebuildSound_ = false;
        InitSoundDialog();
        SetActiveDialog(soundDialog.get());
    }
    if (rebuildVideo_) {
        rebuildVideo_ = false;
        InitVideoDialog();
        SetActiveDialog(videoDialog.get());
    }
    if (rebuildRadio_) {
        rebuildRadio_ = false;
        ShowRadioDialog();
    }
    // Rebuilt so a sold part drops off.
    if (sparePartsNeedsReopen && GetActiveDialog() == nullptr) {
        sparePartsNeedsReopen = false;
        if (CollectSpares().empty()) {
            // Selling out closes with this, not the "no spare parts" line.
            bubbles[kSellerBubble]->SetText(GameText::Msg(GameText::ALL_PARTS_GONE), BUBBLE_RIGHT);
            bubbles[kSellerBubble]->SetVisible(true);
        } else {
            ShowSparePartsDialog();
        }
    }

    currentTime = GameTicks() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);
    UpdateTireChangeAnimation(deltaTime);
    UpdateTransmissionInstallAnimation(deltaTime);
    UpdateEngineBayInstallAnimation(deltaTime);
    UpdateSr2MachineAnimation(deltaTime);
    UpdateSr2Creeper(deltaTime);

    Scene::DrawScene();
    DrawSr2Fixtures(false);

    if (tireChangePhase != TireChangePhase::IDLE && jackAnim && jackLastFrameRect.w > 0) {
        SDL_Rect dstRect = {138, 157, jackLastFrameRect.w, jackLastFrameRect.h};
        jackAnim->DrawWithRects(jackLastFrameRect, dstRect);
    }

    if (initialDelay) {
        if (GameTicks() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            const CarPtr& current = state.GetCurrentCarPtr();
            // Repaint the sign first, or a totaled car's name stays up.
            UpdateSign();
            if (current) {
                if (current == lastParkedCar) {
                    // The car that was already here pops into place instead of driving in again.
                    carBitmap = GetOrCreateCarBitmap(current);
                    SDL_Point idlePos = GetCarIdlePosition();
                    carx = idlePos.x;
                    cary = idlePos.y;
                    carBitmap->Move({carx, cary});
                    UpdateSign();
                } else {
                    needsNewAnimation = true;
                    StartAnimateCar(current, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
                    animatingCar = true;
                }
            }
        }
        return;
    }

    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx;
        p.y = cary - tireChangeLiftY;
        carBitmap->Move(p);
        if (tireChangeLiftY > 0) {
            carBitmap->SetShadowYOffset(tireChangeLiftY);
        }
        if (carBitmap->IsStickerWipeActive()) {
            carBitmap->UpdateStickerWipe(deltaTime);
        }
        carBitmap->Draw();

        if (tireChangePhase != TireChangePhase::IDLE) {
            if (jackAnim) {
                int jackX = IsSr2() ? 138 : (carBitmap->GetRearWheelPos().x + carBitmap->GetFrontWheelPos().x) / 2;
                jackAnim->SetPosition(jackX, (int)jackYPos);
                jackAnim->SetVisible(true);
                jackAnim->Draw();
            }

            switch (tireChangePhase) {
                case TireChangePhase::JACK_SLIDE_UP:
                case TireChangePhase::JACK_CRANK_UP:
                    carBitmap->DrawWheelAt((int)animRearWheelX,  (int)animRearWheelY  - tireChangeLiftY, false);
                    carBitmap->DrawWheelAt((int)animFrontWheelX, (int)animFrontWheelY - tireChangeLiftY, false);
                    break;
                case TireChangePhase::REAR_WHEEL_OUT:
                    carBitmap->DrawWheelAt((int)animRearWheelX,  (int)animRearWheelY,  (int)animRearWheelX  != initialRearWheelPos.x,  true);
                    carBitmap->DrawWheelAt((int)animFrontWheelX, (int)animFrontWheelY, false);
                    break;
                case TireChangePhase::FRONT_WHEEL_OUT:
                    carBitmap->DrawWheelAt((int)animFrontWheelX, (int)animFrontWheelY, (int)animFrontWheelX != initialFrontWheelPos.x, true);
                    break;
                case TireChangePhase::NEW_REAR_WHEEL_IN:
                    carBitmap->DrawWheelAt((int)animRearWheelX,  (int)animRearWheelY,  (int)animRearWheelX  != initialRearWheelPos.x);
                    break;
                case TireChangePhase::NEW_FRONT_WHEEL_IN:
                    carBitmap->DrawWheelAt((int)animRearWheelX,  (int)animRearWheelY,  false);
                    carBitmap->DrawWheelAt((int)animFrontWheelX, (int)animFrontWheelY, (int)animFrontWheelX != initialFrontWheelPos.x);
                    break;
                case TireChangePhase::JACK_CRANK_DOWN:
                case TireChangePhase::JACK_SLIDE_DOWN:
                    carBitmap->DrawWheelAt((int)animRearWheelX,  (int)animRearWheelY, false);
                    carBitmap->DrawWheelAt((int)animFrontWheelX, (int)animFrontWheelY, false);
                    break;
                default:
                    break;
            }
        }

        CarMetadata_t metadata =
            state.carData.GetCarMetadata(carBitmap->GetCar()->GetModelRef().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;

        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect("rear_bumper", rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect("roof", choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect("front_bumper", frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect("transmission", transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect("engine", engineRect);
    }


    DrawSr2Fixtures(true);

    DrawDebugZones();

    SDL_RenderCopy(renderer, signTexture, NULL, NULL);

    if (paintCanPreview && paintCanPreview->IsVisible()) {
        paintCanPreview->Draw();
    }

    if (stickerNeedsRefresh) {
        stickerNeedsRefresh = false;
        stickerDialog->Hide();
        ShowStickerDialog();
    }

    // The install window stays visible under its spare-transmission picker.
    if (transmissionInstallDialog && transmissionInstallDialog->IsVisible() &&
        activeDialog != transmissionInstallDialog.get()) {
        transmissionInstallDialog->Draw();
    }

    // Same for the engine bay under its part pickers.
    bool engineBayPickerActive =
        (engineSwapDialog && activeDialog == engineSwapDialog.get()) ||
        (manifoldSwapDialog && activeDialog == manifoldSwapDialog.get()) ||
        (carbSwapDialog && activeDialog == carbSwapDialog.get());
    if (engineBay && engineBayPickerActive && engineBay->GetDialog()->IsVisible()) {
        engineBay->GetDialog()->Draw();
    }
    if (sr2BayDirty) {
        sr2BayDirty = false;
        Sr2BayRebuild();
    }
    if (sr2BayList && activeDialog == sr2BayList.get() && sr2BayDialog && sr2BayDialog->IsVisible()) {
        sr2BayDialog->Draw();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    // This one has to stay up until acknowledged.
    if (outOfDough) bubbles[kOutOfDoughBubble]->SetVisible(true);

    for (auto& b : bubbles) {
        b->Draw();
    }
}

void Garage::OnFadeComplete() {
    DropUnownedCarBitmaps();
    // A purchase or save load may have replaced the car it last synced to.
    RefreshEngineBayGeometry();
    CarScene::OnFadeComplete();
}

// With the car gone, nothing to sell and no money, the run is over.
// The main loop charges two hours for the cruise out and two for the way back (SR1 0x2432d0, SR2 lmain.c 0x257ec6).
void Garage::StartTrip() {
    state.AdvanceTicks(300);
    tripReturnTicks = 300;
}

void Garage::OnSceneEnter() {
    CarScene::OnSceneEnter();
    state.AdvanceTicks(tripReturnTicks);
    tripReturnTicks = 0;
    outOfDough = state.IsOutOfDough();
    LOG_INFO("Garage: enter - car=%d owned=%zu spares(t/x/m/c/e)=%zu/%zu/%zu/%zu/%zu "
             "bank=%d -> outOfDough=%d",
             state.GetCurrentCar() != nullptr ? 1 : 0,
             state.GetOwnedCars().size(),
             state.GetSpareTires().size(), state.GetSpareTransmissions().size(),
             state.GetSpareManifolds().size(), state.GetSpareCarburetors().size(),
             state.GetSpareEngines().size(),
             state.GetBankroll(), outOfDough ? 1 : 0);
    if (outOfDough) {
        bubbles[kOutOfDoughBubble]->SetVisible(true);
        LOG_INFO("Garage: out of dough - the run is over");
    }
}

void Garage::OnSceneExit() {
    lastParkedCar = carBitmap ? state.GetCurrentCarPtr() : CarPtr{};
    CarScene::OnSceneExit();
}

void Garage::SetActiveDialog(Dialog *dialog) {
    CarScene::SetActiveDialog(dialog);

    // Show() turned every bitmap on; hide the unused mounts, then whatever is apart.
    if (engineBay && dialog == engineBay->GetDialog()) {
        engineBay->OnDialogShown();
        ApplyEngineBayAssemblyState();
    }
}

bool Garage::PushEvent(SDL_Event* event) {
    // Once the run is over, the next key or click rolls the hall of fame.
    if (outOfDough) {
        if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {
            outOfDough = false;
            bubbles[kOutOfDoughBubble]->SetVisible(false);
            state.MoveToScene(SCENE_KING_OF_COUNTY);
        }
        return true;
    }
    if (activeDialog && activeDialog->IsVisible()) {
        return activeDialog->HandleEvent(event);
    }
    // Right-click on the radio opens the song control dialog. Disabled until #42.
    if (false) {
    //if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT) {
        static const SDL_Rect radioRect = {259, 61, 25, 12};
        int mx = event->button.x, my = event->button.y;
        if (mx >= radioRect.x && mx < radioRect.x + radioRect.w &&
            my >= radioRect.y && my < radioRect.y + radioRect.h) {
            AudioManager* am = state.GetAssets().GetAudioManager();
            if (am && am->HasPCSpeaker()) {
                ShowRadioDialog();
                return true;
            }
        }
    }
    return CarScene::PushEvent(event);
}

void Garage::OnCarAnimationComplete() {
    LOG_INFO("Garage::OnCarAnimationComplete - Entry point");
    CarScene::OnCarAnimationComplete();
    if (pendingCar != nullptr) {
        state.SetCurrentCar(pendingCar);

        UpdateSign();
        RefreshEngineBayGeometry();

        needsNewAnimation = true;

        LOG_INFO("Garage::OnCarAnimationComplete - Starting drive-in animation for new car");
        StartAnimateCar(pendingCar, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);

        pendingCar = nullptr;
    }

    if (carBitmap) {
        StandardCarColor newColor = carBitmap->GetCurrentColor();
        LOG_INFO("OnCarAnimationComplete: carBitmap color is %d, updating paint can", (int)newColor);
        previewColor = newColor;
        originalColor = previewColor;
        UpdatePaintCanPreview();
    }

    UpdateCarZones();
}

void Garage::UpdateCarZones() {
    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();

        // From the car being drawn: carBitmap keeps it alive, and State's current car may already differ.
        CarMetadata_t metadata =
            state.carData.GetCarMetadata(carBitmap->GetCar()->GetModelRef().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;

        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect("rear_bumper", rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect("roof", choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect("front_bumper", frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect("transmission", transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect("engine", engineRect);
    }
}

CarBitmap* Garage::GetOrCreateCarBitmap(const CarPtr& car) {
    auto it = carBitmaps.find(car);
    if (it == carBitmaps.end()) {
        it = carBitmaps.emplace(car, std::make_unique<CarBitmap>(car, nullptr, car->GetColor())).first;
    }
    return it->second.get();
}

void Garage::DropUnownedCarBitmaps() {
    // Cars can be bought, sold or replaced by a save load without the Garage knowing. Housekeeping only:
    // cars are shared-owned, so a stale entry never dangles.
    const std::vector<CarPtr>& owned = state.GetOwnedCars();
    for (auto it = carBitmaps.begin(); it != carBitmaps.end(); ) {
        if (std::find(owned.begin(), owned.end(), it->first) == owned.end()) {
            // Or a New Game would leave the previous car on screen.
            if (it->second.get() == carBitmap) carBitmap = nullptr;
            it = carBitmaps.erase(it);
        } else {
            ++it;
        }
    }
    FreeCarListSwatches();
    sortedOwnedCars.clear();
}

void Garage::FreeCarListSwatches() {
    for (auto* s : carListSwatches) {
        SDL_FreeSurface(s);
    }
    carListSwatches.clear();
}

Garage::~Garage() {
    FreeCarListSwatches();
    carBitmaps.clear();
}

void Garage::StartAnimateCar(CarPtr car, Driver* driver, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;
    if (state.GetCurrentCar() == nullptr) return;

    LOG_INFO("Garage::StartAnimateCar - car=%p, type=%d", (void*)car.get(), animType);

    carBitmap = GetOrCreateCarBitmap(state.GetCurrentCarPtr());
    cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), state.GetCurrentCar()->GetModel().height);

    if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
        SDL_Point idlePos = GetCarIdlePosition();
        carx = idlePos.x;
        LOG_INFO("Garage::StartAnimateCar - Reverse animation, car at idle x=%d", carx);
    } else {
        carx = -GAME_WIDTH;
        LOG_INFO("Garage::StartAnimateCar - Drive in animation, car off-screen x=%d", carx);
    }

    carBitmap->Move({carx, cary});

    CarScene::StartAnimateCar(std::move(car), driver, animType);

    UpdateSign();

    currentAnimationType = animType;
    LOG_INFO("Garage::StartAnimateCar - Animation started");
}
