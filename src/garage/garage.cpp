#include "garage.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "ui/checkbox.h"
#include "assets.h"
#include "state.h"
#include "engine_bay.h"
#include "embedded_loader.h"
#include "audio/audio_manager.h"
#include "engine_part.h"
#include "parsers/libp.h"
#include <SDL_image.h>
#include <algorithm>

Garage::Garage() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {

   SDL_Rect bottleRect = {7, 161, 8, 24};
    Zone bottle(ZONE_GARAGE_DRINK, strdup("Wanna drink ?"), &bottleRect);

    SDL_Rect newsPaperRect = {15, 158, 44, 25};
    Zone newsPaper(ZONE_GARAGE_NEWSPAPER, strdup("Check out the newspaper"), &newsPaperRect);

    SDL_Rect changeTiresRect = {136, 157, 26, 29};
    Zone changeTires(ZONE_GARAGE_TIRES, strdup("Change tires"), &changeTiresRect);

    SDL_Rect newPaintJobRect = {228, 160, 33, 24};
    Zone newPaintJob(ZONE_GARAGE_PAINT_JOB, strdup("New paint job - $20"), &newPaintJobRect);

    SDL_Rect gasRect = {288, 161, 23, 26};
    Zone gas(ZONE_GARAGE_GAS_CAN, strdup("Wanna tank ?"), &gasRect);

    SDL_Rect carInfoRect = {290, 107, 27, 14};
    Zone carInfo(ZONE_GARAGE_CAR_INFO, strdup("Car info"), &carInfoRect);

    SDL_Rect calendarRect = {299, 56, 16, 45};
    Zone calendar(ZONE_GARAGE_CALENDAR, strdup("Calendar"), &calendarRect);

    SDL_Rect radioRect = {259, 61, 25, 12};
    Zone radio(ZONE_GARAGE_MUSIC, strdup("Squelsh it !"), &radioRect);

    SDL_Rect stickersRect = {216, 63, 17, 10};
    Zone stickers(ZONE_GARAGE_STICKERS, strdup("Stickers - $5"), &stickersRect);

    SDL_Rect partsRect = {203, 79, 17, 19};
    Zone parts(ZONE_GARAGE_SELL_PARTS, strdup("Sell parts"), &partsRect);

    SDL_Rect menuRect = {156, 65, 33, 14};
    Zone menu(ZONE_GARAGE_MENU, strdup("Time to quit"), &menuRect);

    SDL_Rect carsRect = {69, 41, 200, 16};
    Zone cars(ZONE_GARAGE_CAR_LIST, strdup("Cars"), &carsRect);

    SDL_Rect streetRect = {0, 42, 40, 62};
    Zone street(ZONE_GARAGE_GOTO_DINER, strdup("Hit the street"), &streetRect);

    SDL_Rect rearBumperRect = {290, 107, 27, 14};
    Zone rearBumper(ZONE_GARAGE_REAR_BUMPER, strdup("Rear bumper"), &rearBumperRect);

    SDL_Rect frontBumperRect = {290, 107, 27, 14};
    Zone frontBumper(ZONE_GARAGE_FRONT_BUMPER, strdup("Front bumper"), &frontBumperRect);

    SDL_Rect roofRect = {290, 107, 27, 14};
    Zone roof(ZONE_GARAGE_CHOPPED_ROOF, strdup("Roof"), &roofRect);

    SDL_Rect transmissionRect = {290, 107, 27, 14};
    Zone transmission(ZONE_GARAGE_TRANSMISSION, strdup("Transmission"), &transmissionRect);

    SDL_Rect engineRect = {290, 107, 27, 14};
    Zone engine(ZONE_GARAGE_ENGINE, strdup("Pop the hood"), &engineRect);

    signRect = {72, 44, 195, 12};

    zones.push_back(bottle);
    zones.push_back(newsPaper);
    zones.push_back(changeTires);
    zones.push_back(newPaintJob);
    zones.push_back(gas);
    zones.push_back(carInfo);
    zones.push_back(calendar);
    zones.push_back(radio);
    zones.push_back(stickers);
    zones.push_back(parts);
    zones.push_back(menu);
    zones.push_back(cars);
    zones.push_back(street);
    zones.push_back(rearBumper);
    zones.push_back(frontBumper);
    zones.push_back(roof);
    zones.push_back(transmission);
    zones.push_back(engine);

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
    auto roofConstraintBubble = std::make_unique<Bubble>("  This car can\'t be chopped!", BUBBLE_RIGHT, 103, 30, true);
    roofConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(roofConstraintBubble));
    auto bumperConstraintBubble = std::make_unique<Bubble>("  This car can\'t be stripped!", BUBBLE_RIGHT, 103, 30, true);
    bumperConstraintBubble->SetVisible(false);
    bubbles.push_back(std::move(bumperConstraintBubble));

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
    Zone switchItZone(ZONE_GARAGE_BUTTON_SWITCH_IT, strdup("Switch it"), &switchItRect);
    Zone sellItZone(ZONE_GARAGE_BUTTON_SELL_IT, strdup("Sell it"), &sellItRect);
    Zone forgetItZone(ZONE_GARAGE_BUTTON_FORGET_IT, strdup("Forget it"), &forgetItRect);
    Zone scrollTopZone(ZONE_GARAGE_BUTTON_SCROLL_TOP, strdup("\\"), &scrollTopRect);
    Zone scrollDownZone(ZONE_GARAGE_BUTTON_SCROLL_DOWN, strdup(">"), &scrollDownRect);
    Zone scrollUpZone(ZONE_GARAGE_BUTTON_SCROLL_UP, strdup("<"), &scrollUpRect);
    Zone scrollBottomZone(ZONE_GARAGE_BUTTON_SCROLL_BOTTOM, strdup("/"), &scrollBottomRect);

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
        this->hud->SetText(zone.GetText());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Switch it
                int selectedIndex = carSelectDialog->GetSelectedIndex();
                if (selectedIndex >= 0) {
                    Car* selectedCar = sortedOwnedCars[selectedIndex];
                    if (selectedCar != state.GetCurrentCar()) {
                        pendingCar = selectedCar;
                        StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                    }
                }
                carSelectDialog->Hide();
                break;
            }
            case 1: // Sell it
                carSelectDialog->Hide();
                break;
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
                Car* selectedCar = sortedOwnedCars[selectedIndex];
            LOG_INFO("Garage::ListDialog: Selected index=%d, car=%s (ptr: %p)",
                   selectedIndex,
                   selectedCar->GetModel().name.c_str(),
                   (void*)selectedCar);

            // Update paint can to show the selected car's color
            previewColor = selectedCar->GetColor();
            originalColor = previewColor;
            LOG_INFO("Car list: Updating paint can to color %d for selected car", (int)previewColor);
            UpdatePaintCanPreview();

            // Only switch if selecting a different car
            if (selectedCar != state.GetCurrentCar()) {
                pendingCar = selectedCar;
                carSelectDialog->Hide();  // Hide dialog immediately when switching cars
                StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                LOG_INFO("Garage::ListDialog: Switching from current car (ptr: %p) to pending car (ptr: %p)",
                       (void*)state.GetCurrentCar(),
                       (void*)pendingCar);
            } else {
                LOG_INFO("Garage::ListDialog: Same car selected, ignoring");
            }
        },
        listBoxRect,
        selectedItemRect,
        {0xFF, 0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0x00, 0xFF},
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
                        engineBay->SwitchToState(ENGINE_BAY_TUNING);
                        SetActiveDialog(engineBay->GetDialog());
                    } else {
                        engineBay->SwitchToState(ENGINE_BAY_OVERVIEW);
                        SetActiveDialog(engineBay->GetDialog());
                    }
                    break;
                case 1: // Parts
                    engineBay->SwitchToState(ENGINE_BAY_PARTS);
                    SetActiveDialog(engineBay->GetDialog());
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
            this->hud->SetText(zone.GetText());
        }
    );

    InitCarBitmaps();

    // Set initial car bitmap
    if (state.GetCurrentCar()) {
        carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    }

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
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, state.GetCurrentCar()->GetModel().index);
    return {x, y};
}

void Garage::UpdateSign() {

    LOG_INFO("Garage::UpdateSign");
    char text[50];
    CarModel m = state.GetCurrentCar()->GetModel();
    snprintf(text, 50, "%s.", m.name.c_str());

    // Clear the surface
    Uint32 transparent = SDL_MapRGBA(signSurface->format, 0, 0, 0, 0);
    SDL_FillRect(signSurface, NULL, transparent);

    // Write new text
    int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), text);
    int x = signRect.x + (signRect.w - textWidth) / 2;
    SFont_Write(signSurface, state.GetAssets().GetMainFont(), x, 45, text);

    // Destroy old texture if it exists
    if (signTexture) {
        SDL_DestroyTexture(signTexture);
    }

    // Create new texture
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureBlendMode(signTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
}

void Garage::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }
    if (tireChangePhase != TireChangePhase::IDLE) {
        return;
    }

    switch (zone)
    {
    case ZONE_GARAGE_GOTO_DINER:
        state.MoveToScene(SCENE_DINER);
        break;
    case ZONE_GARAGE_NEWSPAPER:
        state.MoveToScene(SCENE_NEWSPAPER);
        break;
    case ZONE_GARAGE_GAS_CAN:
        state.MoveToScene(SCENE_GAS_STATION);
        break;
    case ZONE_GARAGE_DRINK:
        bubbles[0]->SetVisible(true);
        break;
    case ZONE_GARAGE_SELL_PARTS:
        bubbles[1]->SetVisible(true);
        break;
    case ZONE_GARAGE_TIRES:
        if (state.GetSpareTires().empty()) {
            bubbles[2]->SetVisible(true);
        } else {
            ShowTireDialog();
        }
        break;
    case ZONE_GARAGE_CAR_LIST: {
        sortedOwnedCars.clear();
        for (auto& car : state.GetOwnedCars()) {
            sortedOwnedCars.push_back(car);
        }
        std::sort(sortedOwnedCars.begin(), sortedOwnedCars.end(), [](Car* a, Car* b) {
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
        break;
    }
    case ZONE_GARAGE_CAR_INFO:
        ShowCarInfoDialog();
        break;
    case ZONE_GARAGE_MENU:
        SetActiveDialog((Dialog*)gameMenuDialog.get());
        break;
    case ZONE_GARAGE_ENGINE:
        engineBay->ResetToOverview();
        SetActiveDialog((Dialog*)engineBay->GetDialog());
        break;
    case ZONE_GARAGE_FRONT_BUMPER:
        ShowBumperDialog(true);  // true = front bumper
        break;
    case ZONE_GARAGE_REAR_BUMPER:
        ShowBumperDialog(false);  // false = rear bumper
        break;
    case ZONE_GARAGE_CHOPPED_ROOF:
        ShowRoofDialog();
        break;
    case ZONE_GARAGE_PAINT_JOB:
        ShowPaintDialog();
        break;
    case ZONE_GARAGE_STICKERS:
        stickerScrollRow = 0;
        ShowStickerDialog();
        break;
    case ZONE_GARAGE_CALENDAR:
        ShowCalendarDialog();
        break;
    default:
        break;
    }
}

void Garage::DrawScene() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);
    UpdateTireChangeAnimation(deltaTime);

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
            needsNewAnimation = true;  // Ensure parent's initialization logic runs

            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
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

        // get clickboxes
        CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;

        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect(ZONE_GARAGE_REAR_BUMPER, &rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect(ZONE_GARAGE_CHOPPED_ROOF, &choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect(ZONE_GARAGE_FRONT_BUMPER, &frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect(ZONE_GARAGE_TRANSMISSION, &transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect(ZONE_GARAGE_ENGINE, &engineRect);
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

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    // Draw bubbles last so they appear on top of dialogs
    for (auto& b : bubbles) {
        b->Draw();
    }
}

void Garage::OnFadeComplete() {
    CarScene::OnFadeComplete();
}

void Garage::SetActiveDialog(Dialog *dialog) {
    // Call the base class implementation
    CarScene::SetActiveDialog(dialog);

    // If this is the engine bay dialog, restore the correct visibility
    if (dialog == engineBay->GetDialog()) {
        engineBay->OnDialogShown();
    }
}

bool Garage::PushEvent(SDL_Event* event) {
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return true;
    }
    return CarScene::PushEvent(event);
}

void Garage::OnCarAnimationComplete() {
    LOG_INFO("Garage::OnCarAnimationComplete - Entry point");
    CarScene::OnCarAnimationComplete();
    if (pendingCar != nullptr) {
        state.SetCurrentCar(pendingCar);

        UpdateSign();

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

        // get clickboxes
        CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;

        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect(ZONE_GARAGE_REAR_BUMPER, &rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect(ZONE_GARAGE_CHOPPED_ROOF, &choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect(ZONE_GARAGE_FRONT_BUMPER, &frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect(ZONE_GARAGE_TRANSMISSION, &transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect(ZONE_GARAGE_ENGINE, &engineRect);
    }
}

void Garage::InitCarBitmaps() {
    // Create bitmaps for all owned cars
    for (auto& car : state.GetOwnedCars()) {
        if (carBitmaps.find(car) == carBitmaps.end()) {
            carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
        }
    }
}

CarBitmap* Garage::GetOrCreateCarBitmap(Car* car) {
    auto it = carBitmaps.find(car);
    if (it == carBitmaps.end()) {
        carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
    }
    return carBitmaps[car].get();
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

void Garage::StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) {
    if (animatingCar && carBitmap) return;

    LOG_INFO("Garage::StartAnimateCar - car=%p, type=%d", (void*)car, animType);

    // Garage-specific setup BEFORE calling parent
    // Create/get car bitmap
    carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), state.GetCurrentCar()->GetModel().index);

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
    CarScene::StartAnimateCar(car, driver, animType);

    // Update sign for initial car
    UpdateSign();

    currentAnimationType = animType;
    LOG_INFO("Garage::StartAnimateCar - Animation started");
}
