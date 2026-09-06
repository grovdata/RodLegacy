#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"
#include "embedded_loader.h"
#include "embedded_resources.h"
#include "ui/main_font.h"
#include "audio/audio_manager.h"
#include "engine_part.h"
#include "parsers/libp.h"
#include "scene_layout.h"
#include <SDL_image.h>
#include <algorithm>

Garage::Garage() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {
    signRect = {72, 44, 195, 12};

    zones = SceneLayout::Load(embedded_garage_toml);

    GetZone("drink").onClick = [this] {
        if (!CanInteract()) return;
        bubbles[0]->SetVisible(true);
    };
    GetZone("newspaper").onClick = [this] {
        if (!CanInteract()) return;
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
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
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
    GetZone("stickers").onClick = [this] {
        if (!CanInteract()) return;
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        stickerScrollRow = 0;
        ShowStickerDialog();
    };
    GetZone("sell_parts").onClick = [this] {
        if (!CanInteract()) return;
        bool anySpareParts = !state.GetSpareEngines().empty() ||
                              !state.GetSpareTransmissions().empty() ||
                              !state.GetSpareManifolds().empty() ||
                              !state.GetSpareCarburetors().empty();
        if (!anySpareParts) bubbles[1]->SetVisible(true);
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
        if (!state.GetCurrentCar()) { bubbles[7]->SetVisible(true); return; }
        // A car with its engine apart doesn't go anywhere.
        if (!state.GetCurrentCar()->IsRunning()) { bubbles[13]->SetVisible(true); return; }
        state.MoveToScene(SCENE_DINER);
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
        // The window opens whether or not you own a spare - you may just want
        // to unscrew what's in there, and a car that dropped its transmission
        // in a race has to be able to get in to fit a new one. "No spares" is
        // SELECT's problem, same as the engine bay's pickers.
        ShowTransmissionInstallDialog();
    };
    GetZone("engine").onClick = [this] {
        if (!CanInteract()) return;
        engineBay->ResetToOverview();
        SetActiveDialog((Dialog*)engineBay->GetDialog());
    };

    auto bubbleDrink = std::make_unique<Bubble>("  Don\'t drink and drive, bub!", BUBBLE_LEFT, 103, 30, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(std::move(bubbleDrink));
    auto partsBubble = std::make_unique<Bubble>("  You\'ve got no spare parts, dummy!", BUBBLE_LEFT, 93, 30, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(std::move(partsBubble));
    auto tiresBubble = std::make_unique<Bubble>("  You\'ve got no spare tires, dummy!", BUBBLE_LEFT, 79, 30, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(std::move(tiresBubble));
    auto paintBubble = std::make_unique<Bubble>("  Paint ain\'t free!", BUBBLE_RIGHT, 103, 30, true);
    paintBubble->SetVisible(false);
    bubbles.push_back(std::move(paintBubble));
    auto customizationBubble = std::make_unique<Bubble>("  Customization ain\'t free!", BUBBLE_RIGHT, 103, 30, true);
    customizationBubble->SetVisible(false);
    bubbles.push_back(std::move(customizationBubble));
    // Original string, 0x247226. The backtick renders as a space.
    auto roofConstraintBubble = std::make_unique<Bubble>("  Ya can\'t chop this roof, dummy`!", BUBBLE_RIGHT, 103, 30, true);
    roofConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(roofConstraintBubble));
    auto bumperConstraintBubble = std::make_unique<Bubble>("  This car can\'t be stripped!", BUBBLE_RIGHT, 103, 30, true);
    bumperConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(bumperConstraintBubble));
    auto noCarBubble = std::make_unique<Bubble>("  You\'ve got no car, dummy!", BUBBLE_LEFT, 103, 30, true);
    noCarBubble->SetVisible(false);
    bubbles.push_back(std::move(noCarBubble));
    // The backtick renders as a space (see roofConstraintBubble above).
    auto transNotEmptyBubble = std::make_unique<Bubble>("Where will you put it, speedy`?\nMake some room first !", BUBBLE_RIGHT_TWO_LINES, 80, 30, true);
    transNotEmptyBubble->SetVisible(false);
    bubbles.push_back(std::move(transNotEmptyBubble));
    auto noSpareEngineBubble = std::make_unique<Bubble>("  You\'ve got no spare engines, dummy!", BUBBLE_LEFT, 93, 30, true);
    noSpareEngineBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareEngineBubble));
    auto noSpareManifoldBubble = std::make_unique<Bubble>("  You\'ve got no spare manifolds, dummy!", BUBBLE_LEFT, 93, 30, true);
    noSpareManifoldBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareManifoldBubble));
    auto noSpareCarbBubble = std::make_unique<Bubble>("  You\'ve got no spare carburetors, dummy!", BUBBLE_LEFT, 93, 30, true);
    noSpareCarbBubble->SetVisible(false);
    bubbles.push_back(std::move(noSpareCarbBubble));
    auto partIncompatibleBubble = std::make_unique<Bubble>("  Pay attention, dummy! That won\'t fit!", BUBBLE_LEFT, 93, 30, true);
    partIncompatibleBubble->SetVisible(false);
    bubbles.push_back(std::move(partIncompatibleBubble));
    // bubbles[13] - shown when the player tries to leave with the engine apart.
    auto notRunningBubble = std::make_unique<Bubble>("  Your car\'s not running, speedy`!", BUBBLE_LEFT, 93, 30, true);
    notRunningBubble->SetVisible(false);
    bubbles.push_back(std::move(notRunningBubble));
    // bubbles[14] - every line the car buyer says; the text is set per
    // message by ShowHaggleMessage (garage_sell_car.cpp).
    // Placed for its longest line ("You must be crazy. I'll take it`!"), the
    // same way the other bubbles here are nudged left as their text grows.
    auto haggleBubble = std::make_unique<Bubble>("  No thanks`!", BUBBLE_LEFT, 93, 30, true);
    haggleBubble->SetVisible(false);
    bubbles.push_back(std::move(haggleBubble));

    currentPointer = SrLibAmigaSpriteAssetMousePointerWrench;

    signSurface = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(signSurface, SDL_BLENDMODE_BLEND);

    // Clear the entire sign area initially
    SDL_FillRect(signSurface, NULL, SDL_MapRGBA(signSurface->format, 0, 0, 0, 0));

    // Create initial texture
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);

    // Set audio manager for engine sound
    SetAudioManager(state.GetAssets().GetAudioManager());

    // Don't start animation here - it will be started after initialDelay in Draw()

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

    // Dialog zones
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
            nullptr,  // onClick will be set by Dialog
            true,     // defaultChoice
            0        // index
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

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Your cars", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {40, 39, 240, 1},
    };

    // Add dialog zones to config
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
    dialogConfig.zones = dialogZones;  // Add the dialog zones here
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
                        pendingCar = selectedCar;
                        StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
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

            // Update paint can to show the selected car's color
            previewColor = selectedCar->GetColor();
            originalColor = previewColor;
            LOG_INFO("Car list: Updating paint can to color %d for selected car", (int)previewColor);
            UpdatePaintCanPreview();

            // Only switch if selecting a different car
            if (selectedCar != state.GetCurrentCarPtr()) {
                pendingCar = selectedCar;
                carSelectDialog->Hide();  // Hide dialog immediately when switching cars
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

    // Initialize paint color tracking to current car's color
    StandardCarColor currentCarColor = state.GetCurrentCar() ? state.GetCurrentCar()->GetColor() : CarColorRed;
    previewColor = currentCarColor;
    originalColor = currentCarColor;
    LOG_INFO("Garage constructor: Initializing paint can to current car color %d", (int)currentCarColor);

    // Initialize paint can preview (always visible)
    paintCanPreview = std::make_unique<Bitmap>(10, 12, 2);
    paintCanPreview->SetPosition(233, 166);
    paintCanPreview->SetVisible(true);
    UpdatePaintCanPreview();  // Set initial color

    InitGameMenuDialog();
    InitOptionsDialog();
    InitBumperDialogs();

    engineBay = std::make_unique<EngineBay>(
        state.GetCurrentCar()->GetTuneLevel(),
        state,
        [this](int buttonIndex) {
            switch (buttonIndex) {
                case 0: // Tune
                    if (engineBay->GetCurrentState() == ENGINE_BAY_OVERVIEW) {
                        // Nothing to tune while the engine is in pieces.
                        Car* car = state.GetCurrentCar();
                        if (!car || !car->IsRunning()) {
                            bubbles[13]->SetVisible(true);
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
                    //state.SetGameState(GAME_STATE_GARAGE);
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


    // Load jack animation
    auto* jackBitmap = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, CAR_JACK_ANIMATION});
    if (jackBitmap) {
        const SpriteAnimationInfo* animInfo = GetSpriteAnimationInfo(CAR_JACK_ANIMATION);
        if (animInfo && animInfo->frameCount > 0) {
            SDL_Rect frameRect = {0, 0, CAR_JACK_FRAMES[0].w, CAR_JACK_FRAMES[0].h};
            jackAnim = std::make_unique<AnimatedBitmap>(jackBitmap, frameRect);
            jackAnim->SetAnimationFromInfo(animInfo);
            jackAnim->SetLoop(false);
            jackTotalFrames = animInfo->frameCount;
            // Frame after the animation frames is the background mask
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
    // The garage can be carless - after a New Game, or after selling the car
    // that was parked here - and then the sign simply says nothing.
    Car* current = state.GetCurrentCar();
    char text[50];
    snprintf(text, 50, "%s", current ? current->GetModel().name.c_str() : "");

    // Clear the surface
    Uint32 transparent = SDL_MapRGBA(signSurface->format, 0, 0, 0, 0);
    SDL_FillRect(signSurface, NULL, transparent);

    // Write new text
    if (text[0] != '\0') {
        int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), text);
        int x = signRect.x + (signRect.w - textWidth) / 2;
        SFont_Write(signSurface, state.GetAssets().GetMainFont(), x, 45, text);
    }

    // Destroy old texture if it exists
    if (signTexture) {
        SDL_DestroyTexture(signTexture);
    }

    // Create new texture
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
}


void Garage::DrawScene() {
    if (rebuildOptions_) {
        rebuildOptions_ = false;
        InitOptionsDialog();
        SetActiveDialog(optionsDialog.get());
    }
    if (rebuildRadio_) {
        rebuildRadio_ = false;
        ShowRadioDialog();
    }

    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);
    UpdateTireChangeAnimation(deltaTime);
    UpdateTransmissionInstallAnimation(deltaTime);
    UpdateEngineBayInstallAnimation(deltaTime);

    Scene::DrawScene();

    // Mask the static car jack in the background with the last animation frame
    if (tireChangePhase != TireChangePhase::IDLE && jackAnim && jackLastFrameRect.w > 0) {
        SDL_Rect dstRect = {138, 157, jackLastFrameRect.w, jackLastFrameRect.h};
        jackAnim->DrawWithRects(jackLastFrameRect, dstRect);
    }

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            const CarPtr& current = state.GetCurrentCarPtr();
            // Nothing to drive in before the player's bought their first car.
            if (current) {
                if (current == lastParkedCar) {
                    // Same car that was already sitting here (e.g. just came
                    // back from the newspaper) - pop it back into place
                    // instantly instead of replaying the off-screen drive-in.
                    carBitmap = GetOrCreateCarBitmap(current);
                    SDL_Point idlePos = GetCarIdlePosition();
                    carx = idlePos.x;
                    cary = idlePos.y;
                    carBitmap->Move({carx, cary});
                    UpdateSign();
                } else {
                    needsNewAnimation = true;  // Ensure parent's initialization logic runs
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

        // Draw tire change animation overlays
        if (tireChangePhase != TireChangePhase::IDLE) {
            // Jack: centered between wheel positions, at animated Y
            if (jackAnim) {
                int jackX = (carBitmap->GetRearWheelPos().x + carBitmap->GetFrontWheelPos().x) / 2;
                jackAnim->SetPosition(jackX, (int)jackYPos);
                jackAnim->SetVisible(true);
                jackAnim->Draw();
            }

            // Draw wheels at their individually animated positions
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

        // Click boxes belong to the car being drawn, not to whatever State
        // currently calls the player's car - the two can differ (a carless
        // player still has a bitmap mid-exit-animation), and carBitmap keeps
        // its own car alive, so this is the source that cannot be null here.
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


    DrawDebugZones();

    SDL_RenderCopy(renderer, signTexture, NULL, NULL);

    // Draw paint can preview if paint dialog is active
    if (paintCanPreview && paintCanPreview->IsVisible()) {
        paintCanPreview->Draw();
    }

    // Deferred sticker dialog refresh (avoids use-after-free when scrolling)
    if (stickerNeedsRefresh) {
        stickerNeedsRefresh = false;
        stickerDialog->Hide();
        ShowStickerDialog();
    }

    // The spare-transmissions picker is its own Dialog opened on top of the
    // transmission install window (not a replacement for it): draw the
    // install window here whenever it's open but not the active dialog, so
    // it stays visible underneath the picker.
    if (transmissionInstallDialog && transmissionInstallDialog->IsVisible() &&
        activeDialog != transmissionInstallDialog.get()) {
        transmissionInstallDialog->Draw();
    }

    // Same idea for the engine bay: its own overview dialog stays visible
    // underneath whichever spare-part picker (engine/manifold/carb) is on
    // top of it.
    bool engineBayPickerActive =
        (engineSwapDialog && activeDialog == engineSwapDialog.get()) ||
        (manifoldSwapDialog && activeDialog == manifoldSwapDialog.get()) ||
        (carbSwapDialog && activeDialog == carbSwapDialog.get());
    if (engineBay && engineBayPickerActive && engineBay->GetDialog()->IsVisible()) {
        engineBay->GetDialog()->Draw();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    // Draw bubbles last so they appear on top of dialogs
    for (auto& b : bubbles) {
        b->Draw();
    }
}

void Garage::OnFadeComplete() {
    DropUnownedCarBitmaps();
    // The overview may still be showing the parts of whatever car it last
    // synced to, which a purchase or a save load can have replaced.
    RefreshEngineBayGeometry();
    CarScene::OnFadeComplete();
}

void Garage::OnSceneExit() {
    lastParkedCar = carBitmap ? state.GetCurrentCarPtr() : CarPtr{};
    CarScene::OnSceneExit();
}

void Garage::SetActiveDialog(Dialog *dialog) {
    // Call the base class implementation
    CarScene::SetActiveDialog(dialog);

    // Show() just turned every bitmap in the dialog on - put back the
    // mounts this manifold doesn't use, then whatever is still apart.
    if (dialog == engineBay->GetDialog()) {
        engineBay->OnDialogShown();
        ApplyEngineBayAssemblyState();
    }
}

bool Garage::PushEvent(SDL_Event* event) {
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return true;
    }
    // Right-click on the radio opens the song control dialog.
    if (false) {
    // TODO: Disabled until milestone 0.2
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
        // The new car's engine bay art was frozen at whatever car was
        // current when EngineBay was built - resync now that a different
        // car is actually being driven in.
        RefreshEngineBayGeometry();

        // Mark that we need a new animation (important for parent's StartAnimateCar logic)
        needsNewAnimation = true;

        // Start the drive-in animation for the new car (this will set up audio, bitmap, position, etc.)
        LOG_INFO("Garage::OnCarAnimationComplete - Starting drive-in animation for new car");
        StartAnimateCar(pendingCar, nullptr, CarAnimationType::SINGLE_CAR_DRIVE);

        pendingCar = nullptr;
    }

    // Update paint preview color to match the current car
    if (carBitmap) {
        StandardCarColor newColor = carBitmap->GetCurrentColor();
        LOG_INFO("OnCarAnimationComplete: carBitmap color is %d, updating paint can", (int)newColor);
        previewColor = newColor;
        originalColor = previewColor;
        UpdatePaintCanPreview();
    }

    // Update car zones after animation completes
    UpdateCarZones();
}

void Garage::UpdateCarZones() {
    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();

        // Click boxes belong to the car being drawn, not to whatever State
        // currently calls the player's car - the two can differ (a carless
        // player still has a bitmap mid-exit-animation), and carBitmap keeps
        // its own car alive, so this is the source that cannot be null here.
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
    // Cars can have been bought, sold, or replaced wholesale (a save load)
    // since this scene was last on screen, and nothing tells the Garage that.
    // So it checks for itself on the way in rather than relying on every
    // mutation site to remember to call something.
    //
    // Cars are shared-owned, so an entry for a car that is no longer in the
    // roster is only wasted memory - never a dangling pointer. That makes
    // this housekeeping, not a correctness requirement, which is why it can
    // safely skip the one entry the scene is currently drawing.
    const std::vector<CarPtr>& owned = state.GetOwnedCars();
    for (auto it = carBitmaps.begin(); it != carBitmaps.end(); ) {
        if (std::find(owned.begin(), owned.end(), it->first) == owned.end()) {
            // Stop drawing a car the player no longer has. Without this a
            // New Game (which empties the roster) would leave the previous
            // car on screen, and every "is there a car?" check that reads
            // carBitmap would answer yes while State says otherwise.
            if (it->second.get() == carBitmap) carBitmap = nullptr;
            it = carBitmaps.erase(it);
        } else {
            ++it;
        }
    }
    // Both are rebuilt from State when the car-select list is next opened.
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

    // Garage-specific setup BEFORE calling parent
    // Create/get car bitmap
    carBitmap = GetOrCreateCarBitmap(state.GetCurrentCarPtr());
    cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), state.GetCurrentCar()->GetModel().height);

    // Set initial position based on animation type
    if (animType == CarAnimationType::SINGLE_CAR_REVERSE) {
        // Reverse: start at idle position (car is already visible)
        SDL_Point idlePos = GetCarIdlePosition();
        carx = idlePos.x;
        LOG_INFO("Garage::StartAnimateCar - Reverse animation, car at idle x=%d", carx);
    } else {
        // Drive in: start off-screen left
        carx = -GAME_WIDTH;
        LOG_INFO("Garage::StartAnimateCar - Drive in animation, car off-screen x=%d", carx);
    }

    carBitmap->Move({carx, cary});

    // Call parent method to start engine sound and set up animation
    // Parent will NOT override our position for reverse animations
    CarScene::StartAnimateCar(std::move(car), driver, animType);

    // Update sign for initial car
    UpdateSign();

    currentAnimationType = animType;
    LOG_INFO("Garage::StartAnimateCar - Animation started");
}
