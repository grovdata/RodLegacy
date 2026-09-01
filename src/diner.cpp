#include "log.h"
#include "diner.h"
#include "state.h"
#include "ui/hud.h"
#include "cardata.h"
#include "debug_overlay.h"
#include "animated_bitmap.h"
#include "scene_layout.h"
#include "embedded_resources.h"
#include "ui/main_font.h"
#include <cstdlib>

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;

Diner::Diner() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, DINER_SCREEN}) {
    lastFrameTime = SDL_GetTicks64() / 1000.0;
    carBitmap = nullptr;
    isPaused = false;
    challenge = std::make_unique<Challenge>();

    zones = SceneLayout::Load(embedded_diner_toml);

    GetZone("goto_garage").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        sceneTransitioningTo = SCENE_GARAGE;
        FadeOut();
        state.MoveToScene(SCENE_GARAGE);
    };
    GetZone("call").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        if (state.GetKnownOpponents().empty()) bubbles[0]->SetVisible(true);
        else ShowCallListDialog();
    };
    GetZone("challenge_opponent").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        if (carBitmap && !animatingCar && !exitingCar && currentOpponent)
            challenge->Open(state.GetAssets().GetMainFont());
    };
}

void Diner::Init(bool debugMode) {
    CarScene::Init(debugMode);

    // Set audio manager for engine sound
    SetAudioManager(state.GetAssets().GetAudioManager());

    roadMaskSurface.reset(SDL_CreateRGBSurface(0, 320, 10, 32, 0, 0, 0, 0));
    SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
    roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
    SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);

    auto guyBubble = std::make_unique<Bubble>("  What\'s his name?", BUBBLE_LEFT, 79, 30, true);
    guyBubble->SetVisible(false);
    bubbles.push_back(std::move(guyBubble));

    // Add phonebook guy as AnimatedBitmap to container (only if not already added)
    if (!GetDrawable("phonebook_guy")) {
        Bitmap* phonebookGuySheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, PHONEBOOK_GUY});

        SDL_Rect phonebookGuyRect = {189, 65, 38, 60};
        phonebookGuyAnim = std::make_unique<AnimatedBitmap>(phonebookGuySheet, phonebookGuyRect);
        // PHONEBOOK_GUY is 2 rows, 5 columns according to libp.cpp
        int frameWidth = phonebookGuySheet->GetSurface()->w / 5;
        int frameHeight = phonebookGuySheet->GetSurface()->h / 2;
        phonebookGuyAnim->SetAnimationFrames(frameWidth, frameHeight, 5, 2); // 5 columns, 2 rows
        phonebookGuyAnim->SetSpeed(0.4f); // 0.4s per frame (similar to original 400ms cycle)
        phonebookGuyAnim->SetLoop(true);
        phonebookGuyAnim->Play();

        AddDrawable("phonebook_guy", phonebookGuyAnim.get());
    }

    if (!GetDrawable("waitressLeft")) {
        Bitmap* waitressLeftBitmap = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DINER_WAITRESS_ANIM_LEFT});

        SDL_Rect waitressLeftRect = {25, 68, 40, 41};
        waitressLeftAnim = std::make_unique<AnimatedBitmap>(waitressLeftBitmap, waitressLeftRect);
        waitressLeftAnim->SetAnimationFrames(40, 41, 6, 2);
        waitressLeftAnim->SetSpeed(0.2f);
        waitressLeftAnim->SetLoop(true);
        waitressLeftAnim->Play();
        waitressLeftAnim->SetVisible(false); // Start hidden
        AddDrawable("waitressLeft", waitressLeftAnim.get());
    }

    if (!GetDrawable("waitressRight")) {
        Bitmap* waitressRightBitmap = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DINER_WAITRESS_ANIM_RIGHT});

        SDL_Rect waitressRightRect = {-40, 68, 40, 41}; // Start off-screen left
        waitressRightAnim = std::make_unique<AnimatedBitmap>(waitressRightBitmap, waitressRightRect);
        waitressRightAnim->SetAnimationFrames(40, 41, 6, 2);
        waitressRightAnim->SetSpeed(0.2f);
        waitressRightAnim->SetLoop(true);
        waitressRightAnim->Play();
        waitressRightAnim->SetVisible(true); // Start visible for entering
        AddDrawable("waitressRight", waitressRightAnim.get());
    }

    // Create masked screen background (similar to gas station)
    Bitmap* screenMaskBitmap = state.GetAssets().GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, DINER_SCREEN_MASK});
    if (screenMaskBitmap) {
        maskedScreenBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 256, 41, 32, SDL_PIXELFORMAT_RGBA8888));
        background->CreateMask(screenMaskBitmap->GetSurface(), maskedScreenBackground.get(), {0, 68, 256, 41});
        maskedScreenBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedScreenBackground.get()));
        SDL_SetTextureBlendMode(maskedScreenBackgroundTexture.get(), SDL_BLENDMODE_BLEND);
    }

    hud->SetVisible(true);

    initStartTime = SDL_GetTicks64();
}

Diner::~Diner() {
    // Smart pointers will automatically clean up
    opponentCarBitmaps.clear();
    carBitmap = nullptr;
}

SDL_Point Diner::GetCarIdlePosition() {
    if (!currentOpponent) return {80, 95};
    int x = SrLibQueryGetCarIdleX(SCENE_DINER);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_DINER, currentOpponent->car->GetModel().index);
    return {x, y};
}


void Diner::DrawScene() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    // Update animations - declare variables once for the entire method
    auto phonebookGuy = dynamic_cast<AnimatedBitmap*>(GetDrawable("phonebook_guy"));
    if (phonebookGuy) {
        phonebookGuy->Update(deltaTime);
    }

    auto waitressLeft = dynamic_cast<AnimatedBitmap*>(GetDrawable("waitressLeft"));
    if (waitressLeft) {
        waitressLeft->Update(deltaTime);
    }

    auto waitressRight = dynamic_cast<AnimatedBitmap*>(GetDrawable("waitressRight"));
    if (waitressRight) {
        waitressRight->Update(deltaTime);
    }

    // Update waitress sequence animation
    UpdateWaitressSequence(deltaTime, waitressLeft, waitressRight);

    Scene::DrawScene();
    if (!isSetup) {
        Init(debugMode);
        isSetup = true;
    }

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
        }
        // Still draw everything else while waiting

        // Draw waitress animations first (before mask)
        if (waitressLeft) {
            waitressLeft->Draw();
        }
        if (waitressRight) {
            waitressRight->Draw();
        }

        // Draw the masked screen background (to mask parts of waitress animation)
        if (maskedScreenBackgroundTexture) {
            SDL_Rect screenMaskRect = {0, 68, 256, 41};
            SDL_RenderCopy(renderer, maskedScreenBackgroundTexture.get(), NULL, &screenMaskRect);
        }

        // Draw other drawables (phonebook guy, etc.)
        if (phonebookGuy) {
            phonebookGuy->Draw();
        }

        for (auto& b : bubbles) {
            b->Draw();
        }
        if (debugMode && debugOverlay) {
            DrawLegacyDebugOverlay();
        }
        return;
    }

    // Draw waitress animations first (before mask)
    if (waitressLeft) {
        waitressLeft->Draw();
    }
    if (waitressRight) {
        waitressRight->Draw();
    }

    // Draw the masked screen background (to mask parts of waitress animation)
    if (maskedScreenBackgroundTexture) {
        SDL_Rect screenMaskRect = {0, 68, 256, 41};
        SDL_RenderCopy(renderer, maskedScreenBackgroundTexture.get(), NULL, &screenMaskRect);
    }

    // Draw other drawables (phonebook guy, etc.)
    if (phonebookGuy) {
        phonebookGuy->Draw();
    }

    if (carBitmap != nullptr) {
        carBitmap->Move({carx, cary});
        carBitmap->Draw();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    for (auto& b : bubbles) {
        b->Draw();
    }

    challenge->Draw(static_cast<float>(deltaTime));

    if (challenge->WantsRace()) {
        challenge->Reset();
        FadeOut();
        state.MoveToScene(SCENE_DRIVING);
    }
    if (challenge->WantsCarExit()) {
        challenge->Reset();
        TriggerCarExit();
        debugWaitTimer = 0;
    }

    if (debugMode && debugOverlay) {
        DrawLegacyDebugOverlay();
    }
    hud->Draw();

}

void Diner::DrawMaskAfterCar() {
    // Draw the road mask (for car)
    if (roadMaskSurface && roadMaskTexture) {
        SDL_BlitSurface(surface.get(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
        roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
        SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer, roadMaskTexture.get(), NULL, &roadMaskSrcRect);
    }

    CarScene::DrawMaskAfterCar();
}

std::string Diner::GetOpponentText(Opponent *opponent) {
    std::string result = opponent->driver.name + "    " + opponent->car->GetModel().name;
    return result;
}

void Diner::StartAnimateCar(CarPtr, Driver*, CarAnimationType) {
    if (animatingCar && carBitmap) return;

    this->currentOpponent = state.GetNextOpponent();
    if (!currentOpponent) return;

    state.AddKnownOpponent(currentOpponent);

    std::string oppoText = GetOpponentText(currentOpponent);
    std::string opponentId = currentOpponent->driver.name;

    hud->SetText(oppoText.c_str());
    SetZoneText("background", oppoText);

    auto existingBitmap = opponentCarBitmaps.find(opponentId);
    if (existingBitmap != opponentCarBitmaps.end()) {
        carBitmap = existingBitmap->second.get();
    } else {
        StandardCarColor carColor = static_cast<StandardCarColor>(currentOpponent->car->GetColor());
        auto newBitmap = std::make_unique<CarBitmap>(currentOpponent->car, &currentOpponent->driver, carColor);
        carBitmap = newBitmap.get();
        opponentCarBitmaps[opponentId] = std::move(newBitmap);
    }

    int carYOffset = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), currentOpponent->car->GetModel().index);
    this->cary = carYOffset;

    // Reset state for new opponent
    exitingCar = false;
    debugWaitTimer = 0;
    engineSoundStarted = false;
    LOG_INFO("Diner::StartAnimateCar - Reset state: exitingCar=0, debugWaitTimer=0, engineSoundStarted=0");

    CarScene::StartAnimateCar(currentOpponent->car, &currentOpponent->driver, CarAnimationType::OPPONENT_CONTINUOUS);

    // Audio start for drive-in is handled by CarScene::StartAnimateCar.
}

void Diner::UpdateDebugInfo() {
    if (!debugMode) return;

    // Not gated on debugOverlay: that is only the in-game text widget, and the
    // debug UI reads this without one. Nor cleared afterwards - the base class
    // has already filled in the common scene rows and they are worth keeping.
    CarScene::UpdateDebugInfo();

    AddDebugValue("diner", "");
    AddDebugValue("car pos", std::to_string(carx) + ", " + std::to_string(cary));
    AddDebugValue("paused", isPaused ? "yes" : "no");

    if (currentOpponent == nullptr || carBitmap == nullptr) {
        AddDebugValue("opponent", "none on screen");
        return;
    }

    const CarModel model = currentOpponent->car->GetModel();
    const BitmapOffset_t offset = model.offsets;

    AddDebugValue("opponent", "");
    AddDebugValue("driver", currentOpponent->driver.name);
    AddDebugValue("model", model.name);
    AddDebugValue("color", std::to_string(currentOpponent->car->GetColor()));
    AddDebugValue("roof chopped", currentOpponent->car->IsRoofChopped() ? "yes" : "no");
    AddDebugValue("bumpers stripped",
                  currentOpponent->car->IsBumpersStripped() ? "yes" : "no");

    AddDebugValue("sprite offsets", "");
    AddDebugValue("air scoop", std::to_string(offset.offsetXAirScoop) + ", " +
                               std::to_string(offset.offsetYAirScoop));
    AddDebugValue("rear tire x", std::to_string(offset.offsetXRearTire));
    AddDebugValue("front tire x", std::to_string(offset.offsetXFrontTire));
    AddDebugValue("tires y", std::to_string(carBitmap->GetWheelsYOffset()));
    AddDebugValue("driver", std::to_string(offset.offsetXDriver) + ", " +
                            std::to_string(offset.offsetYDriver));
    AddDebugValue("chopped roof x", std::to_string(offset.offsetXChoppedRoof));
    AddDebugValue("front bumper", std::to_string(offset.offsetXFrontBumper) + ", " +
                                  std::to_string(offset.offsetYFrontBumper));
    AddDebugValue("decal", std::to_string(offset.offsetXDecal) + ", " +
                           std::to_string(offset.offsetYDecal));
}

bool Diner::PushEvent(SDL_Event *event) {
    if (challenge->HandleEvent(event)) return true;

    bool consumed = CarScene::PushEvent(event);
    if (!debugMode) return consumed;

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_p:
                isPaused = !isPaused;
                return true;
            case SDLK_UP:
                if (isPaused && carBitmap) {
                    cary--;
                    carBitmap->MoveBody({carx, cary});
                    return true;
                }
                break;
            case SDLK_DOWN:
                if (isPaused && carBitmap) {
                    cary++;
                    carBitmap->MoveBody({carx, cary});
                    return true;
                }
                break;
            case SDLK_LEFT:
                if (isPaused && carBitmap) {
                    carBitmap->MoveWheelsYOffset(carBitmap->GetWheelsYOffset() - 1);
                    return true;
                }
                break;
            case SDLK_RIGHT:
                if (isPaused && carBitmap) {
                    carBitmap->MoveWheelsYOffset(carBitmap->GetWheelsYOffset() + 1);
                    return true;
                }
                break;
            case SDLK_r:
                if (isPaused && carBitmap) {
                    carBitmap->ToggleChoppedRoof();
                    return true;
                }
                break;
        }
    }
    return consumed;
}

void Diner::FadeOut() {
    // Reset animation state during fade out (safer than during hotkey press)
    OnSceneExit();

    // Call parent implementation
    Scene::FadeOut();
}

void Diner::UpdateCarAnimation(double deltaTime) {
    // Debug logging before calling parent
    static bool lastAnimatingCar = false;
    static bool lastExitingCar = false;
    
    // Call parent implementation first
    CarScene::UpdateCarAnimation(deltaTime);

    // Debug and fix the wait timer for OPPONENT_CONTINUOUS
    if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && !exitingCar && carBitmap) {
        debugWaitTimer += deltaTime;

        // Hold the timer while a challenge dialog is in progress
        if (!CanStartCarExit()) {
            debugWaitTimer = 0;
        }
        // Fix: Manually trigger exit after 3 seconds if base class doesn't do it
        else if (debugWaitTimer >= 3.0) {
            LOG_DEBUG("Diner::UpdateCarAnimation - Manual exit trigger after 3 seconds");
            exitingCar = true;
            debugWaitTimer = 0;  // Reset for next cycle
        }
    } else if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && exitingCar && carBitmap) {
        // Log when we're in exit state
        static double exitLogTimer = 0;
        exitLogTimer += deltaTime;
        if (exitLogTimer >= 1.0) {
            LOG_DEBUG("Diner::UpdateCarAnimation - In exit state: exitingCar=%d, animatingCar=%d", exitingCar, animatingCar);
            exitLogTimer = 0;
        }
    }

    // Check for state changes
    if (exitingCar != lastExitingCar) {
        LOG_DEBUG("Diner::UpdateCarAnimation - exitingCar changed: %d -> %d", lastExitingCar, exitingCar);
        lastExitingCar = exitingCar;
    }
    if (animatingCar != lastAnimatingCar) {
        LOG_DEBUG("Diner::UpdateCarAnimation - animatingCar changed: %d -> %d", lastAnimatingCar, animatingCar);
        lastAnimatingCar = animatingCar;
    }

    // Check if exit animation just started
    if (exitingCar && !engineSoundStarted) {
     
        // Start engine sound when opponent car starts driving away
        if (audioManager) {
            // Set engine period for driving away (similar to reverse out)
            audioManager->UpdateEnginePeriod(605);  // Period 605 = ~5916 Hz
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
            engineSoundStarted = true;
        }
    }

    // Reset flag when exit animation completes
    if (!exitingCar && engineSoundStarted) {
        engineSoundStarted = false;
        LOG_DEBUG("Diner::UpdateCarAnimation - Exit animation completed, resetting engine sound flag");
    }

}

void Diner::OnCarAnimationComplete() {
    LOG_INFO("Diner::OnCarAnimationComplete - Opponent car reached idle position, stopping drive-in engine sound");

    // Stop the drive-in engine sound when car reaches idle position
    if (audioManager) {
        audioManager->StopLoop(0);  // Channel 0 is engine sound
        LOG_INFO("Diner::OnCarAnimationComplete - Drive-in engine sound stopped");
    }
}

void Diner::OnCarExitComplete() {
    LOG_INFO("Diner::OnCarExitComplete - Opponent car exit animation completed");
    // Engine sound is already playing from UpdateCarAnimation
}

void Diner::OnSceneExit() {
    initialDelay = true;
    animatingCar = false;
    exitingCar = false;
    carBitmap = nullptr;
    carx = 0;
    cary = 0;
    currentOpponent = nullptr;

    challenge->Reset();

    // Reset waitress animation state
    waitressState = WAITRESS_ENTERING;
    waitressAnimTime = 0.0;

    // Call parent CarScene implementation (which handles common animation state)
    CarScene::OnSceneExit();
}

void Diner::UpdateWaitressSequence(double deltaTime, AnimatedBitmap* waitressLeft, AnimatedBitmap* waitressRight) {
    if (!waitressLeft || !waitressRight) return;

    waitressAnimTime += deltaTime;

    switch (waitressState) {
        case WAITRESS_ENTERING: {
            // Move waitress right from -40 to 208
            double progress = waitressAnimTime * waitressSpeed;
            int currentX = -40 + (int)progress;

            if (currentX >= 208) {
                // Reached target position, switch to pause
                waitressRight->SetPosition(208, 68);
                waitressRight->SetFrame(10); // Set to frame 11 for pause
                waitressRight->Stop(); // Stop animation during pause
                waitressState = WAITRESS_PAUSED;
                waitressAnimTime = 0.0;
            } else {
                waitressRight->SetPosition(currentX, 68);
            }
            break;
        }

        case WAITRESS_PAUSED: {
            // Wait for pause duration
            if (waitressAnimTime >= waitressPauseTime) {
                // Switch to left animation and start exiting
                waitressRight->SetVisible(false);
                waitressLeft->SetVisible(true);
                waitressLeft->SetPosition(208, 68);
                waitressLeft->Play(); // Restart animation for exiting
                waitressState = WAITRESS_EXITING;
                waitressAnimTime = 0.0;
            }
            break;
        }

        case WAITRESS_EXITING: {
            // Move waitress left from 208 to off-screen left
            double progress = waitressAnimTime * waitressSpeed;
            int currentX = 208 - (int)progress;

            if (currentX <= -40) {
                // Exited screen, hide and reset
                waitressLeft->SetVisible(false);
                waitressLeft->SetPosition(25, 68); // Reset to original position
                waitressRight->SetPosition(-40, 68); // Reset to start position
                waitressRight->SetVisible(true);
                waitressState = WAITRESS_HIDDEN;
                waitressAnimTime = 0.0;
            } else {
                waitressLeft->SetPosition(currentX, 68);
            }
            break;
        }

        case WAITRESS_HIDDEN: {
            // Wait a bit before starting the sequence again
            if (waitressAnimTime >= 5.0) { // 5 second delay before repeating
                waitressRight->Play(); // Restart animation for entering
                waitressState = WAITRESS_ENTERING;
                waitressAnimTime = 0.0;
            }
            break;
        }
    }
}

void Diner::ShowCallListDialog() {
    const auto& known = state.GetKnownOpponents();

    std::vector<std::string> labels;
    for (const auto* opp : known) {
        labels.push_back(opp->driver.name);
    }

    SDL_Rect bounds = {85, 37, 235, 158};

    SDL_Rect listBoxRect = {95, 51, 138, 74};
    SDL_Rect topRect = {86, 38, 148, 88};
    SDL_Rect bottomRect = {86, 126, 148, 33};
    SDL_Rect selectedItemRect = {87, 129, 146, 11};
    SDL_Rect callRect = {93, 140, 64, 14};
    SDL_Rect forgetItRect = {162, 140, 64, 14};

    SDL_Rect scrollTopRect = {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15};
    SDL_Rect scrollUpRect = {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15};
    SDL_Rect scrollDownRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15};
    SDL_Rect scrollBottomRect = {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15};

    Zone callZone("diner_call", "Call", callRect);
    Zone forgetItZone("diner_forget_it", "Forget it", forgetItRect);
    Zone scrollTopZone("diner_scroll_top", std::string(1, MAIN_FONT_SCROLL_TOP), scrollTopRect);
    Zone scrollUpZone("diner_scroll_up", std::string(1, MAIN_FONT_SCROLL_UP), scrollUpRect);
    Zone scrollDownZone("diner_scroll_down", std::string(1, MAIN_FONT_SCROLL_DOWN), scrollDownRect);
    Zone scrollBottomZone("diner_scroll_bottom", std::string(1, MAIN_FONT_SCROLL_BOTTOM), scrollBottomRect);

    std::vector<ButtonConfig> buttons = {
        {callRect, "Call", nullptr, true, 0},
        {forgetItRect, "Forget it", nullptr, false, 1}
    };

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{87, 40, 146, 10}, "Looking for", {0x00, 0x00, 0x00, 0xFF}, true}
    };

    std::vector<SDL_Rect> lines = {
        {87, 49, 146, 1},
    };

    std::vector<Zone> dialogZones = {
        callZone, forgetItZone,
        scrollTopZone, scrollUpZone, scrollDownZone, scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    dialogConfig.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
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
            case 0: { // Call
                int selectedIndex = callListDialog->GetSelectedIndex();
                if (selectedIndex >= 0) {
                    const auto& knownOpps = state.GetKnownOpponents();
                    if (selectedIndex < (int)knownOpps.size()) {
                        Opponent* selected = knownOpps[selectedIndex];
                        state.SetNextOpponent(selected);
                        callListDialog->Hide();
                        SetActiveDialog(nullptr);
                        // Trigger car exit so the called opponent drives in next
                        if (carBitmap && !exitingCar) {
                            exitingCar = true;
                            debugWaitTimer = 0;
                        }
                    }
                }
                break;
            }
            case 1: // Forget it
                callListDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    dialogConfig.texts = texts;

    callListDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            {},
            [](int selectedIndex) {
                LOG_INFO("Diner::CallList: Selected index=%d", selectedIndex);
            },
            listBoxRect,
            selectedItemRect,
            {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect}
        }
    );

    callListDialog->Show();
    SetActiveDialog(callListDialog.get());
}

void Diner::OnFadeComplete() {
    if (initialDelay) {
        // Pick the next opponent from the pool before the car drives in.
        state.PickNextOpponent();
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation immediately for subsequent fades
        if (!animatingCar && !exitingCar && !carBitmap) {
            state.PickNextOpponent();
            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
        }
    }
}
