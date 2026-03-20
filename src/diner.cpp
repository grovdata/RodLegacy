#include "log.h"
#include "diner.h"
#include "state.h"
#include "ui/hud.h"
#include "cardata.h"
#include "debug_overlay.h"
#include "animated_bitmap.h"
#include <cstdlib>

double carAnimationTimeDiner = 0;

const std::vector<std::string> Diner::kInsults = {
    "Blow it out your ear!",
    "Forget it, geek!",
    "Beat it, twerp!",
    "No way, ya nerd!",
    "Get lost!",
    "Give me a break!",
    "Get outta my face!",
    "Buzz off!",
    "Whadda loser!",
    "Chicken!"
};

const std::vector<std::string> Diner::kAcceptances = {
    "You're on!",
    "Alright!",
    "Let's go!",
    "Cool!",
    "O.K.!",
    "Let's hit it!"
};
double carExitAnimationTime = 0;

Diner::Diner() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, DINER_SCREEN}) {
    lastFrameTime = SDL_GetTicks64() / 1000.0;
    carBitmap = nullptr;
    isPaused = false;
}

void Diner::Init(bool debugMode) {
    CarScene::Init(debugMode);

    // Set audio manager for engine sound
    SetAudioManager(state.GetAssets().GetAudioManager());

    roadMaskSurface.reset(SDL_CreateRGBSurface(0, 320, 10, 32, 0, 0, 0, 0));
    SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
    roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
    SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);

    SDL_Rect orderRect = {132, 53, 27, 9};
    Zone order(ZONE_DINER_ORDER, strdup("Order"), &orderRect);

    SDL_Rect opponentRect = {194, 65, 29, 59};
    Zone opponent(ZONE_DINER_CALL, strdup("Look for opponent"), &opponentRect);

    SDL_Rect garageRect = {0, 170, 320, 30};
    Zone garage(ZONE_DINER_GOTO_GARAGE, strdup("Back to the garage"), &garageRect);

    SDL_Rect backgroundRect = {0, 0, 320, 169};
    Zone backgroundZone(ZONE_DINER_BACKGROUND, strdup(""), &backgroundRect);

    // Zone covering the parked opponent car area (car idles at x=82 in diner)
    SDL_Rect challengeRect = {82, 85, 200, 75};
    Zone challengeZone(ZONE_DINER_CHALLENGE_OPPONENT, strdup("Challenge!"), &challengeRect);

    zones.push_back(order);
    zones.push_back(opponent);
    zones.push_back(garage);
    zones.push_back(challengeZone);
    zones.push_back(backgroundZone);

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

void Diner::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    if (zone == ZONE_DINER_GOTO_GARAGE) {
        sceneTransitioningTo = SCENE_GARAGE;
        FadeOut();
        state.MoveToScene(SCENE_GARAGE);
    } else if (zone == ZONE_DINER_CALL) {
        if (state.GetKnownOpponents().empty()) {
            bubbles[0]->SetVisible(true);
        } else {
            ShowCallListDialog();
        }
    } else if (zone == ZONE_DINER_CHALLENGE_OPPONENT) {
        if (carBitmap && !animatingCar && !exitingCar && currentOpponent
                && challengeStep == ChallengeStep::NONE
                && acceptTimer < 0.0f && insultTimer < 0.0f) {
            ShowRaceTypeDialog();
        }
    }
}

void Diner::Draw() {
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

    Scene::Draw();
    if (!isSetup) {
        Init(debugMode);
        isSetup = true;
    }

    Uint64 elapsed = SDL_GetTicks64() - initStartTime;

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
            animatingCar = true;
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
            debugOverlay->Draw();
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

    // Process deferred dialog transitions (button callbacks must not reset the dialog
    // from inside HandleEvent, so they only set flags which we act on here).
    if (pendingShowInsult) {
        pendingShowInsult = false;
        pendingChallengeStep = ChallengeStep::NONE;
        challengeStep = ChallengeStep::NONE;
        if (challengeDialog) challengeDialog->Hide();
        ShowOpponentSpeech(kInsults[rand() % kInsults.size()]);
    } else if (pendingStartRace) {
        pendingStartRace = false;
        pendingChallengeStep = ChallengeStep::NONE;
        challengeStep = ChallengeStep::NONE;
        if (challengeDialog) challengeDialog->Hide();
        StartChallengeRace();
    } else if (pendingChallengeStep != ChallengeStep::NONE) {
        ChallengeStep next = pendingChallengeStep;
        pendingChallengeStep = ChallengeStep::NONE;
        switch (next) {
            case ChallengeStep::DRAG_BET: ShowDragBetDialog(); break;
            case ChallengeStep::ROAD_BET: ShowRoadBetDialog(); break;
            default: break;
        }
    }

    if (opponentBubble) {
        opponentBubble->Draw();
    }

    if (challengeDialog && challengeDialog->IsVisible()) {
        challengeDialog->Draw();
    }

    if (debugMode && debugOverlay) {
        debugOverlay->Draw();
    }
    hud->Draw();

    // Accept timer: transition to driving after opponent accepts the challenge
    if (acceptTimer >= 0.0f) {
        acceptTimer += static_cast<float>(deltaTime);
        if (acceptTimer >= 1.5f) {
            acceptTimer = -1.0f;
            FadeOut();
            state.MoveToScene(SCENE_DRIVING);
        }
    }

    // Insult timer: trigger opponent car exit after insult is shown
    if (insultTimer >= 0.0f) {
        insultTimer += static_cast<float>(deltaTime);
        if (insultTimer >= 2.0f) {
            insultTimer = -1.0f;
            if (opponentBubble) {
                opponentBubble->SetVisible(false);
            }
            exitingCar = true;
            debugWaitTimer = 0;
        }
    }
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

void Diner::StartAnimateCar(Car*, Driver*, CarAnimationType) {
    if (animatingCar && carBitmap) return;
    
    this->currentOpponent = state.GetNextOpponent();
    if (!currentOpponent) return;

    state.AddKnownOpponent(currentOpponent);

    std::string oppoText = GetOpponentText(currentOpponent);
    std::string opponentId = currentOpponent->driver.name;
    
    hud->SetText(strdup(oppoText.c_str()));
    SetZoneText(ZONE_DINER_BACKGROUND, strdup(oppoText.c_str()));

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
    
    // Start engine sound for opponent driving in
    if (audioManager) {
        // Set engine period for driving in (similar to garage drive-in)
        audioManager->UpdateEnginePeriod(320);  // Period 320 = ~11186 Hz (high pitch)
        audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
        LOG_INFO("Diner::StartAnimateCar - Engine sound started for opponent drive-in");
    }
}

void Diner::UpdateDebugInfo() {
    if (!debugMode) return;
    
    CarScene::UpdateDebugInfo();
        
    if (debugOverlay) {
        debugOverlay->Clear();
        
        debugOverlay->AddLine("Car X: " + std::to_string(carx) + " Y: " + std::to_string(cary));        

        if (currentOpponent && carBitmap) {        
            debugOverlay->AddLine("Opponent: " + currentOpponent->driver.name);
            CarModel model = currentOpponent->car->GetModel();
            BitmapOffset_t offset = model.offsets;
            debugOverlay->AddLine("model: " + model.name + " color: " + std::to_string(currentOpponent->car->GetColor()));
            debugOverlay->AddLine("roof chopped: " + std::string(currentOpponent->car->IsRoofChopped() ? "Yes" : "No"));
            debugOverlay->AddLine("bumpers stripped: " + std::string(currentOpponent->car->IsBumpersStripped() ? "Yes" : "No"));
            debugOverlay->AddLine("offsetXAirScoop: " + std::to_string(offset.offsetXAirScoop) + " Y: " + std::to_string(offset.offsetYAirScoop));            
            debugOverlay->AddLine("offsetXRearTire: " + std::to_string(offset.offsetXRearTire));
            debugOverlay->AddLine("offsetXFrontTire: " + std::to_string(offset.offsetXFrontTire));
            debugOverlay->AddLine("offsetYTires: " + std::to_string(carBitmap->GetWheelsYOffset()));
            debugOverlay->AddLine("offsetXDriver: " + std::to_string(offset.offsetXDriver) + " Y: " + std::to_string(offset.offsetYDriver));            
            debugOverlay->AddLine("offsetXChoppedRoof: " + std::to_string(offset.offsetXChoppedRoof));
            debugOverlay->AddLine("offsetXFrontBumper: " + std::to_string(offset.offsetXFrontBumper) + " Y: " + std::to_string(offset.offsetYFrontBumper));            
            debugOverlay->AddLine("offsetXDecal: " + std::to_string(offset.offsetXDecal) + " Y: " + std::to_string(offset.offsetYDecal));
            
        }
        
        debugOverlay->AddLine("Paused: " + std::string(isPaused ? "Yes" : "No"));
    }
}

bool Diner::PushEvent(SDL_Event *event) {
    // Route events to challenge dialog before the scene processes them
    if (challengeDialog && challengeDialog->IsVisible()) {
        challengeDialog->HandleEvent(event);
        return true;
    }

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
    static double debugTimer = 0;
    debugTimer += deltaTime;
    
    if (debugTimer >= 1.0) {  // Log every second
        LOG_INFO("Diner::UpdateCarAnimation - animatingCar=%d, exitingCar=%d, carBitmap=%p, currentAnimationType=%d", 
                animatingCar, exitingCar, (void*)carBitmap, (int)currentAnimationType);
        debugTimer = 0;
    }
    
    // Call parent implementation first
    CarScene::UpdateCarAnimation(deltaTime);
    
    // Debug and fix the wait timer for OPPONENT_CONTINUOUS
    if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && !exitingCar && carBitmap) {
        debugWaitTimer += deltaTime;
        if (debugWaitTimer >= 0.5) {  // Log every 0.5 seconds
            LOG_INFO("Diner::UpdateCarAnimation - Waiting for exit: waitTimer=%.1f, WAIT_BEFORE_EXIT=3.0", debugWaitTimer);
        }
        
        // Fix: Manually trigger exit after 3 seconds if base class doesn't do it
        if (debugWaitTimer >= 3.0) {
            LOG_INFO("Diner::UpdateCarAnimation - Manual exit trigger after 3 seconds");
            exitingCar = true;
            debugWaitTimer = 0;  // Reset for next cycle
        }
    } else if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && exitingCar && carBitmap) {
        // Log when we're in exit state
        static double exitLogTimer = 0;
        exitLogTimer += deltaTime;
        if (exitLogTimer >= 1.0) {
            LOG_INFO("Diner::UpdateCarAnimation - In exit state: exitingCar=%d, animatingCar=%d", exitingCar, animatingCar);
            exitLogTimer = 0;
        }
    }
    
    // Check for state changes
    if (exitingCar != lastExitingCar) {
        LOG_INFO("Diner::UpdateCarAnimation - exitingCar changed: %d -> %d", lastExitingCar, exitingCar);
        lastExitingCar = exitingCar;
    }
    if (animatingCar != lastAnimatingCar) {
        LOG_INFO("Diner::UpdateCarAnimation - animatingCar changed: %d -> %d", lastAnimatingCar, animatingCar);
        lastAnimatingCar = animatingCar;
    }
    
    // Check if exit animation just started
    if (exitingCar && !engineSoundStarted) {
        LOG_INFO("Diner::UpdateCarAnimation - Opponent car starting to drive away, starting engine sound");
        
        // Start engine sound when opponent car starts driving away
        if (audioManager) {
            // Set engine period for driving away (similar to reverse out)
            audioManager->UpdateEnginePeriod(605);  // Period 605 = ~5916 Hz
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
            engineSoundStarted = true;
            LOG_INFO("Diner::UpdateCarAnimation - Engine sound started for opponent exit");
        }
    }
    
    // Reset flag when exit animation completes
    if (!exitingCar && engineSoundStarted) {
        engineSoundStarted = false;
        LOG_INFO("Diner::UpdateCarAnimation - Exit animation completed, resetting engine sound flag");
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

    // Reset challenge dialog state
    challengeStep = ChallengeStep::NONE;
    pendingChallengeStep = ChallengeStep::NONE;
    pendingStartRace = false;
    pendingShowInsult = false;
    if (challengeDialog) {
        challengeDialog->Hide();
    }
    opponentBubble.reset();
    acceptTimer = -1.0f;
    insultTimer = -1.0f;

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

static Dialog* MakeChallengeDialog(const std::vector<std::string>& labels,
                                    SFont_Font* font,
                                    std::function<void(int)> onPressed)
{
    // Layout: dialog centered horizontally, buttons stacked vertically
    const int dlgX = 80, dlgW = 160;
    const int btnX = dlgX + 10, btnW = dlgW - 20, btnH = 14;
    const int topPad = 6, spacing = 3;
    int dlgH = topPad + static_cast<int>(labels.size()) * (btnH + spacing) - spacing + topPad;
    int dlgY = (200 - dlgH) / 2;

    SDL_Rect bounds = {dlgX, dlgY, dlgW, dlgH};

    DialogConfig cfg;
    cfg.font = font;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.onButtonPressed = onPressed;

    int y = dlgY + topPad;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        SDL_Rect r = {btnX, y, btnW, btnH};
        cfg.buttons.push_back({r, labels[i], nullptr, i == 0, i});
        y += btnH + spacing;
    }

    auto* dlg = new Dialog(cfg);
    dlg->Show();
    return dlg;
}

void Diner::ShowRaceTypeDialog() {
    challengeStep = ChallengeStep::RACE_TYPE;
    challengeDialog.reset(MakeChallengeDialog(
        {"Road Race", "Drag Race", "Forget it"},
        state.GetAssets().GetMainFont(),
        [this](int idx) {
            // Defer: never reset challengeDialog from inside HandleEvent
            switch (idx) {
                case 0: pendingChallengeStep = ChallengeStep::ROAD_BET; break;
                case 1: pendingChallengeStep = ChallengeStep::DRAG_BET; break;
                case 2: pendingShowInsult = true; break;
            }
        }
    ));
}

void Diner::ShowDragBetDialog() {
    challengeStep = ChallengeStep::DRAG_BET;
    challengeDialog.reset(MakeChallengeDialog(
        {"Just for kicks", "$25", "$50", "Forget it"},
        state.GetAssets().GetMainFont(),
        [this](int idx) {
            if (idx == 3) {
                pendingShowInsult = true;
                return;
            }
            static const int bets[] = {0, 25, 50};
            state.SetPendingRace(RaceType::Drag, bets[idx], false);
            pendingStartRace = true;
        }
    ));
}

void Diner::ShowRoadBetDialog() {
    challengeStep = ChallengeStep::ROAD_BET;
    challengeDialog.reset(MakeChallengeDialog(
        {"$25", "$100", "Pink slips", "Forget it"},
        state.GetAssets().GetMainFont(),
        [this](int idx) {
            if (idx == 3) {
                pendingShowInsult = true;
                return;
            }
            static const int bets[] = {25, 100, 0};
            bool pinkSlip = (idx == 2);
            state.SetPendingRace(RaceType::Road, bets[idx], pinkSlip);
            pendingStartRace = true;
        }
    ));
}

void Diner::ShowOpponentSpeech(const std::string& text) {
    opponentBubble = std::make_unique<Bubble>(text, BUBBLE_LEFT, 85, 90, false);
    opponentBubble->SetVisible(true);
    insultTimer = 0.0f;
}

void Diner::StartChallengeRace() {
    ShowOpponentSpeech(kAcceptances[rand() % kAcceptances.size()]);
    acceptTimer = 0.0f;
    insultTimer = -1.0f;  // Not an insult, don't trigger car exit
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

    Zone callZone(ZONE_DINER_BUTTON_CALL, strdup("Call"), &callRect);
    Zone forgetItZone(ZONE_DINER_BUTTON_FORGET_IT, strdup("Forget it"), &forgetItRect);
    Zone scrollTopZone(ZONE_DINER_SCROLL_TOP, strdup("\\"), &scrollTopRect);
    Zone scrollUpZone(ZONE_DINER_SCROLL_UP, strdup("<"), &scrollUpRect);
    Zone scrollDownZone(ZONE_DINER_SCROLL_DOWN, strdup(">"), &scrollDownRect);
    Zone scrollBottomZone(ZONE_DINER_SCROLL_BOTTOM, strdup("/"), &scrollBottomRect);

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
        this->hud->SetText(zone.GetText());
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
            {0xFF, 0xFF, 0xFF, 0xFF},
            {0xFF, 0xFF, 0x00, 0xFF},
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
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
            animatingCar = true;
        }
    }
}