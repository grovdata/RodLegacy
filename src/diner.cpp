#include "diner.h"
#include "state.h"
#include "ui/hud.h"
#include "ui/bubble.h"
#include "cardata.h"
#include "debug_overlay.h"
#include "animated_bitmap.h"

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;

Diner::Diner() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, DINER_SCREEN}) {
    lastFrameTime = SDL_GetTicks64() / 1000.0;
    carBitmap = nullptr;
    isPaused = false;
}

void Diner::Init(bool debugMode) {
    CarScene::Init(debugMode);

    // Set audio manager for engine sound
    SetAudioManager(state.assets.GetAudioManager());

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

    zones.push_back(order);
    zones.push_back(opponent);
    zones.push_back(garage);
    zones.push_back(backgroundZone);

    Bubble *guyBubble = new Bubble("  What\'s his name?", BUBBLE_LEFT, 79, 30, true);
    guyBubble->SetVisible(false);
    bubbles.push_back(guyBubble);

    // Add phonebook guy as AnimatedBitmap to container (only if not already added)
    if (!GetDrawable("phonebook_guy")) {
        Bitmap* phonebookGuySheet = state.assets.GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, PHONEBOOK_GUY});
        
        SDL_Rect phonebookGuyRect = {189, 65, 38, 60};
        auto phonebookGuyAnim = new AnimatedBitmap(phonebookGuySheet, phonebookGuyRect);
        // PHONEBOOK_GUY is 2 rows, 5 columns according to libp.cpp
        // Calculate frame size: width/5, height/2
        int frameWidth = phonebookGuySheet->GetSurface()->w / 5;
        int frameHeight = phonebookGuySheet->GetSurface()->h / 2;
        phonebookGuyAnim->SetAnimationFrames(frameWidth, frameHeight, 5, 2); // 5 columns, 2 rows
        phonebookGuyAnim->SetSpeed(0.4f); // 0.4s per frame (similar to original 400ms cycle)
        phonebookGuyAnim->SetLoop(true);
        phonebookGuyAnim->Play();
        
        AddDrawable("phonebook_guy", std::shared_ptr<IDrawable>(phonebookGuyAnim));
    }

    if (!GetDrawable("waitressLeft")) {
        Bitmap* waitressLeftBitmap = state.assets.GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DINER_WAITRESS_ANIM_LEFT});
        
        SDL_Rect waitressLeftRect = {25, 68, 40, 41};
        auto waitressLeftAnim = new AnimatedBitmap(waitressLeftBitmap, waitressLeftRect);
        waitressLeftAnim->SetAnimationFrames(40, 41, 6, 2);
        waitressLeftAnim->SetSpeed(0.2f);
        waitressLeftAnim->SetLoop(true);
        waitressLeftAnim->Play();
        waitressLeftAnim->SetVisible(false); // Start hidden
        AddDrawable("waitressLeft", std::shared_ptr<IDrawable>(waitressLeftAnim));
    }
    
    if (!GetDrawable("waitressRight")) {
        Bitmap* waitressRightBitmap = state.assets.GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DINER_WAITRESS_ANIM_RIGHT});
        
        SDL_Rect waitressRightRect = {-40, 68, 40, 41}; // Start off-screen left
        auto waitressRightAnim = new AnimatedBitmap(waitressRightBitmap, waitressRightRect);
        waitressRightAnim->SetAnimationFrames(40, 41, 6, 2);
        waitressRightAnim->SetSpeed(0.2f);
        waitressRightAnim->SetLoop(true);
        waitressRightAnim->Play();
        waitressRightAnim->SetVisible(true); // Start visible for entering
        AddDrawable("waitressRight", std::shared_ptr<IDrawable>(waitressRightAnim));
    }

    // Create masked screen background (similar to gas station)
    Bitmap* screenMaskBitmap = state.assets.GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, DINER_SCREEN_MASK});
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
        bubbles[0]->SetVisible(true);
    }
}

void Diner::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    // Update animations - declare variables once for the entire method
    auto phonebookGuy = std::dynamic_pointer_cast<AnimatedBitmap>(GetDrawable("phonebook_guy"));
    if (phonebookGuy) {
        phonebookGuy->Update(deltaTime);
    }

    auto waitressLeft = std::dynamic_pointer_cast<AnimatedBitmap>(GetDrawable("waitressLeft"));
    if (waitressLeft) {
        waitressLeft->Update(deltaTime);
    }

    auto waitressRight = std::dynamic_pointer_cast<AnimatedBitmap>(GetDrawable("waitressRight"));
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
        
        for (auto b : bubbles) {
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

    for (auto b : bubbles) {
        b->Draw();
    }

    if (debugMode && debugOverlay) {
        debugOverlay->Draw();
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

void Diner::StartAnimateCar(Car*, Driver*, CarAnimationType) {
    if (animatingCar && carBitmap) return;
    
    this->currentOpponent = state.GetNextOpponent();
    if (!currentOpponent) return;
    
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
    SDL_Log("Diner::StartAnimateCar - Reset state: exitingCar=0, debugWaitTimer=0, engineSoundStarted=0");
    
    CarScene::StartAnimateCar(currentOpponent->car, &currentOpponent->driver, CarAnimationType::OPPONENT_CONTINUOUS);
    
    // Start engine sound for opponent driving in
    if (audioManager) {
        // Set engine period for driving in (similar to garage drive-in)
        audioManager->UpdateEnginePeriod(320);  // Period 320 = ~11186 Hz (high pitch)
        audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
        SDL_Log("Diner::StartAnimateCar - Engine sound started for opponent drive-in");
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

void Diner::PushEvent(SDL_Event *event) {
    CarScene::PushEvent(event);  // Handle parent events first (like F12)

    if (!debugMode) return;  // Only handle these events in debug mode

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_p:
                isPaused = !isPaused;
                break;
            
            case SDLK_UP:
                if (isPaused && carBitmap) {
                    cary--;  // Move car up
                    carBitmap->MoveBody({carx, cary});
                }
                break;
                
            case SDLK_DOWN:
                if (isPaused && carBitmap) {
                    cary++;  // Move car down
                    carBitmap->MoveBody({carx, cary});
                }
                break;

            case SDLK_LEFT:
                if (isPaused && carBitmap) {
                    carBitmap->MoveWheelsYOffset(carBitmap->GetWheelsYOffset() - 1);
                }
                break;

            case SDLK_RIGHT:
                if (isPaused && carBitmap) {
                    carBitmap->MoveWheelsYOffset(carBitmap->GetWheelsYOffset() + 1);
                }
                break;

            case SDLK_r:
                if (isPaused && carBitmap) {
                    carBitmap->ToggleChoppedRoof();
                }
                break;
        }
    }
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
        SDL_Log("Diner::UpdateCarAnimation - animatingCar=%d, exitingCar=%d, carBitmap=%p, currentAnimationType=%d", 
                animatingCar, exitingCar, (void*)carBitmap, (int)currentAnimationType);
        debugTimer = 0;
    }
    
    // Call parent implementation first
    CarScene::UpdateCarAnimation(deltaTime);
    
    // Debug and fix the wait timer for OPPONENT_CONTINUOUS
    if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && !exitingCar && carBitmap) {
        debugWaitTimer += deltaTime;
        if (debugWaitTimer >= 0.5) {  // Log every 0.5 seconds
            SDL_Log("Diner::UpdateCarAnimation - Waiting for exit: waitTimer=%.1f, WAIT_BEFORE_EXIT=3.0", debugWaitTimer);
        }
        
        // Fix: Manually trigger exit after 3 seconds if base class doesn't do it
        if (debugWaitTimer >= 3.0) {
            SDL_Log("Diner::UpdateCarAnimation - Manual exit trigger after 3 seconds");
            exitingCar = true;
            debugWaitTimer = 0;  // Reset for next cycle
        }
    } else if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && exitingCar && carBitmap) {
        // Log when we're in exit state
        static double exitLogTimer = 0;
        exitLogTimer += deltaTime;
        if (exitLogTimer >= 1.0) {
            SDL_Log("Diner::UpdateCarAnimation - In exit state: exitingCar=%d, animatingCar=%d", exitingCar, animatingCar);
            exitLogTimer = 0;
        }
    }
    
    // Check for state changes
    if (exitingCar != lastExitingCar) {
        SDL_Log("Diner::UpdateCarAnimation - exitingCar changed: %d -> %d", lastExitingCar, exitingCar);
        lastExitingCar = exitingCar;
    }
    if (animatingCar != lastAnimatingCar) {
        SDL_Log("Diner::UpdateCarAnimation - animatingCar changed: %d -> %d", lastAnimatingCar, animatingCar);
        lastAnimatingCar = animatingCar;
    }
    
    // Check if exit animation just started
    if (exitingCar && !engineSoundStarted) {
        SDL_Log("Diner::UpdateCarAnimation - Opponent car starting to drive away, starting engine sound");
        
        // Start engine sound when opponent car starts driving away
        if (audioManager) {
            // Set engine period for driving away (similar to reverse out)
            audioManager->UpdateEnginePeriod(605);  // Period 605 = ~5916 Hz
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.5f);
            engineSoundStarted = true;
            SDL_Log("Diner::UpdateCarAnimation - Engine sound started for opponent exit");
        }
    }
    
    // Reset flag when exit animation completes
    if (!exitingCar && engineSoundStarted) {
        engineSoundStarted = false;
        SDL_Log("Diner::UpdateCarAnimation - Exit animation completed, resetting engine sound flag");
    }
    
}

void Diner::OnCarAnimationComplete() {
    SDL_Log("Diner::OnCarAnimationComplete - Opponent car reached idle position, stopping drive-in engine sound");
    
    // Stop the drive-in engine sound when car reaches idle position
    if (audioManager) {
        audioManager->StopLoop(0);  // Channel 0 is engine sound
        SDL_Log("Diner::OnCarAnimationComplete - Drive-in engine sound stopped");
    }
}

void Diner::OnCarExitComplete() {
    SDL_Log("Diner::OnCarExitComplete - Opponent car exit animation completed");
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
    
    // Reset waitress animation state
    waitressState = WAITRESS_ENTERING;
    waitressAnimTime = 0.0;
    
    // Call parent CarScene implementation (which handles common animation state)
    CarScene::OnSceneExit();
}

void Diner::UpdateWaitressSequence(double deltaTime, std::shared_ptr<AnimatedBitmap> waitressLeft, std::shared_ptr<AnimatedBitmap> waitressRight) {
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

void Diner::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation immediately for subsequent fades
        if (!animatingCar && !exitingCar && !carBitmap) {
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
            animatingCar = true;
        }
    }
}