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
#include "utils.h"
#include "diner_actors.h"
#include "engine_bay.h"
#include "roster.h"
#include <cstdlib>
#include <algorithm>
#include <random>

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
    GetZone("pop_hood").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        if (!carBitmap || animatingCar || exitingCar || !currentOpponent) return;
        // One look per challenger; a second try just annoys him.
        if (hoodPopped) { challenge->Taunt(); return; }
        hoodPopped = true;
        hoodTimer = 0.0f;
        // Rebuilt per opponent - the parts drawn are baked in at construction.
        hoodView = std::make_unique<EngineBayView>(*currentOpponent->car, state,
                                                   [this] { CloseHoodView(); });
        SetActiveDialog(hoodView->GetDialog());
    };
    GetZone("challenge_opponent").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        if (!carBitmap || animatingCar || exitingCar || !currentOpponent) return;
        if (state.GetLastChallengerPoolIndex() == RACER_KING_SLOT) {
            challenge->OpenKing(state.GetAssets().GetMainFont());
        } else {
            challenge->Open(state.GetAssets().GetMainFont());
        }
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

    LoadActors();

    jailBars = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, JAIL});
    jailBars->SetPosition(0, 0);
    jailBars->SetVisible(true);

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
    int y = SrLibQueryGetCarRoofYOffset(SCENE_DINER, currentOpponent->car->GetModel().height);
    return {x, y};
}


using namespace srdiner;

void Diner::LoadActors() {
    if (guyAnim[0]) return;

    const AmigaLibAsset guySheets[2] = {PHONEBOOK_GUY_1, PHONEBOOK_GUY};
    for (int s = 0; s < 2; ++s) {
        Bitmap* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, guySheets[s]});
        guyAnim[s] = std::make_unique<AnimatedBitmap>(sheet, SDL_Rect{kGuyX, kGuyBottomY - kGuyRowH[0], kGuyFrameW, kGuyRowH[0]});
        // The original blits a fixed 40x60/64 cell out of both sheets, but they
        // are not the same width (192 and 200), so the last column of the
        // narrower one runs off the edge. Clip the cell to what is there rather
        // than stretching 32 pixels of art across 40.
        const int sheetW = sheet->GetSurface()->w;
        const int sheetH = sheet->GetSurface()->h;
        std::vector<SDL_Rect> frames;
        for (int cell = 0; cell < 10; ++cell) {
            const int row = cell / 5;
            const int x = (cell % 5) * kGuyFrameW;
            const int y = row == 0 ? 0 : kGuyRowH[0];
            frames.push_back({x, y, std::min(kGuyFrameW, sheetW - x), std::min(kGuyRowH[row], sheetH - y)});
        }
        guyAnim[s]->SetAnimationFrames(frames);
        guyAnim[s]->Stop();   // stepped from the table, never free-running
    }
    ApplyGuyFrame();   // puts him at the phone on his resting frame

    const AmigaLibAsset waitressSheets[2] = {DINER_WAITRESS_ANIM_RIGHT, DINER_WAITRESS_ANIM_LEFT};
    for (int d = 0; d < 2; ++d) {
        Bitmap* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, waitressSheets[d]});
        const int fw = sheet->GetSurface()->w / kWaitressColumns;
        const int fh = sheet->GetSurface()->h / 2;
        waitressAnim[d] = std::make_unique<AnimatedBitmap>(sheet, SDL_Rect{kWaitressStartX[d], kWaitressY, fw, fh});
        std::vector<SDL_Rect> frames;
        // 11 frames out of the 6x2 grid; the twelfth cell is never used.
        for (int i = 0; i < kWaitressFrameCount; ++i) {
            frames.push_back({(i % kWaitressColumns) * fw, (i / kWaitressColumns) * fh, fw, fh});
        }
        waitressAnim[d]->SetAnimationFrames(frames);
        waitressAnim[d]->Stop();
        waitressAnim[d]->SetVisible(false);
    }
}

void Diner::StartWaitressWalk(int dir) {
    waitressDir = dir;
    waitressStep = 0;
    waitressStepTimer = 0.0f;
    ApplyWaitressFrame();
}

void Diner::ApplyWaitressFrame() {
    for (int d = 0; d < 2; ++d) waitressAnim[d]->SetVisible(waitressStep >= 0 && d == waitressDir);
    if (waitressStep < 0) return;
    waitressAnim[waitressDir]->SetFrame(kWaitressFrames[waitressStep] - 1);
    SDL_Rect r = waitressAnim[waitressDir]->GetPosition();
    waitressAnim[waitressDir]->SetPosition(WaitressX(waitressDir, waitressStep), kWaitressY);
    waitressAnim[waitressDir]->SetDimensions(r.w, r.h);
}

void Diner::ApplyGuyFrame() {
    // He stands at the phone the whole time. Frame 0 is his resting pose -
    // the sequence both starts and ends on it.
    const int frame = guyStep >= 0 ? kGuyFrames[guyStep] : 0;
    const int sheet = frame / 10;
    for (int s = 0; s < 2; ++s) guyAnim[s]->SetVisible(s == sheet);
    guyAnim[sheet]->SetFrame(frame % 10);
    // Draw the cell at its own size, so a clipped one is not scaled up. He is
    // bottom-aligned, so a shorter cell sits lower.
    const SDL_Rect src = guyAnim[sheet]->GetCurrentFrameRect();
    guyAnim[sheet]->SetPosition(kGuyX, kGuyBottomY - src.h + 1);
    guyAnim[sheet]->SetDimensions(src.w, src.h);
}

void Diner::UpdateActors(double deltaTime) {
    if (!guyAnim[0]) return;
    const float dtMs = static_cast<float>(deltaTime) * 1000.0f;

    // Phone guy: one table step per hold, then off screen again.
    if (guyStep >= 0) {
        guyHoldTimer += dtMs;
        const float hold = kGuyHold[kGuyFrames[guyStep]] * GUY_STEP_MS;
        if (guyHoldTimer >= hold) {
            guyHoldTimer -= hold;
            if (++guyStep >= kGuySteps) guyStep = -1;
        }
        ApplyGuyFrame();
    }

    // Waitress: one table step per tick, X from the run-length table.
    if (waitressStep >= 0) {
        waitressStepTimer += dtMs;
        while (waitressStepTimer >= WAITRESS_STEP_MS) {
            waitressStepTimer -= WAITRESS_STEP_MS;
            if (++waitressStep >= kWaitressSteps) {
                waitressStep = -1;
                // Walking in leaves her standing; walking out clears her.
                waitressOnScreen = (waitressDir == 0);
                if (waitressTurnAround) {
                    waitressTurnAround = false;
                    StartWaitressWalk(1);
                }
                break;
            }
        }
        ApplyWaitressFrame();
        // She stays on screen on the standing frame after walking in.
        if (waitressStep < 0 && waitressOnScreen) {
            waitressAnim[0]->SetVisible(true);
        }
    }

    if (waitressStep >= 0 || guyStep >= 0) return;

    // The rolls. Percentages are the original's; the cadence is not, since it
    // rolls once per pass of the challenger dialog.
    dinerRollTimer += static_cast<float>(deltaTime);
    if (dinerRollTimer < ROLL_SECONDS) return;
    dinerRollTimer = 0.0f;

    std::uniform_int_distribution<int> roll100(0, 99);
    // The waitress never shows in jail: her scheduler lives in the challenger
    // dialog, which the original's jail loop never reaches. The phone guy does.
    if (state.IsJailed()) {
        if (roll100(RandomEngine()) < 6) {
            guyStep = 0;
            guyHoldTimer = 0.0f;
            ApplyGuyFrame();
        }
        return;
    }
    if (!waitressOnScreen) {
        // Odds climb the longer she has been away, as the original's do as the
        // dialog goes on; it also refuses to bring her back straight away.
        ++waitressWaits;
        const int chance = std::min(85, 5 + 10 * (waitressWaits - 3));
        if (waitressWaits >= 3 && roll100(RandomEngine()) < chance) {
            waitressWaits = 0;
            waitressTurnAround = roll100(RandomEngine()) < 10;  // walks straight through
            StartWaitressWalk(0);
            return;
        }
    } else if (roll100(RandomEngine()) < 20) {
        waitressOnScreen = false;
        StartWaitressWalk(1);
        return;
    }

    if (roll100(RandomEngine()) < 6) {
        guyStep = 0;
        guyHoldTimer = 0.0f;
        ApplyGuyFrame();
    }
}

void Diner::DrawScene() {
    // No status bar in a cell.
    hud->SetVisible(!state.IsJailed());

    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    UpdateActors(deltaTime);

    // He shuts the hood himself after a while - no studying it at leisure.
    if (IsHoodViewOpen()) {
        hoodTimer += static_cast<float>(deltaTime);
        if (hoodTimer >= HOOD_SECONDS) CloseHoodView();
    }

    // Only ever one of each pair is visible; the other draws nothing.
    AnimatedBitmap* waitressLeft = waitressAnim[1].get();
    AnimatedBitmap* waitressRight = waitressAnim[0].get();

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

        // Draw the phone guy (the invisible half of the pair is a no-op)
        for (auto& g : guyAnim) g->Draw();

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

    // Draw the phone guy (the invisible half of the pair is a no-op)
    for (auto& g : guyAnim) g->Draw();

    if (carBitmap != nullptr) {
        carBitmap->Move({carx, cary});
        carBitmap->Draw();
    }

    // The hood window is the exception: it reaches down into the road strip,
    // so DrawMaskAfterCar draws it once the mask is down.
    if (activeDialog && activeDialog->IsVisible() && !IsHoodViewOpen()) {
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
        // ShowOpponentRefusal (0x0023e904): the very next opponent has a
        // 1/100 chance of being The King, win or lose reputation-wise.
        if (challenge->WasOpponentRefusal()) state.RequestKingCameoRoll();
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

    // The bay window's bottom rows overlap the road strip, so it goes on after
    // the mask rather than with the other dialogs in DrawScene.
    if (IsHoodViewOpen()) hoodView->GetDialog()->Draw();

    // Last of all, so nothing draws over them - this runs from DrawExtra,
    // after DrawScene has finished. Everything in the diner is behind the
    // bars: the challengers keep arriving and leaving, you just watch.
    if (state.IsJailed()) jailBars->Draw();
}

bool Diner::IsHoodViewOpen() const {
    return hoodView && hoodView->GetDialog()->IsVisible();
}

// Not hoodView.reset(): this also runs from inside the view's own dialog
// callback, which would delete the object being iterated.
void Diner::CloseHoodView() {
    if (!hoodView) return;
    hoodView->GetDialog()->Hide();
    SetActiveDialog(nullptr);
    hoodTimer = 0.0f;
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

    int carYOffset = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(),
                                                  currentOpponent->car->GetModel().height);
    this->cary = carYOffset;

    // The hood hotspot follows the car: sprite widths and heights differ per
    // model, so anchor it on the air-scoop offset at the idle position.
    const SDL_Point idle = GetCarIdlePosition();
    const BitmapOffset_t hoodOffsets = currentOpponent->car->GetModel().offsets;
    SetZoneRect("pop_hood", {idle.x + hoodOffsets.offsetXAirScoop - 24,
                             idle.y + hoodOffsets.offsetYAirScoop - 4, 64, 26});

    // Reset state for new opponent
    hoodPopped = false;
    exitingCar = false;
    debugWaitTimer = 0;
    exitCrankTimer = -1.0;
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

std::vector<DebugPanel> Diner::GetDebugPanels() {
    std::vector<DebugPanel> panels = CarScene::GetDebugPanels();
    DebugPanel jail{"Diner", {}};
    jail.toggles.push_back({"Jailed (bars, no interaction)", state.DebugJailedFlag()});
    jail.lines = {
        "Jail is normally entered by losing a race to a cop",
        "with less money than the fine. Untick to leave;",
        "a key press in the game view ends the run instead.",
    };
    panels.push_back(std::move(jail));
    return panels;
}

bool Diner::PushEvent(SDL_Event *event) {
    // In jail nothing is clickable - the original passes 1 to its challenger
    // calls, which is look-don't-touch. Watch as long as you like; a key ends
    // the run and rolls the hall of fame.
    if (state.IsJailed()) {
        if (event->type == SDL_KEYDOWN) {
            // MoveToScene reads the flag to build the hall of fame as a loss;
            // clear it after, or leaving the hall locks the diner again.
            // ponytail: a proper end-of-run reset belongs to the game-over
            // sequence (issue #24), this only avoids the dead end.
            state.MoveToScene(SCENE_KING_OF_COUNTY);
            state.SetJailed(false);
        }
        return true;
    }

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
        if (exitCrankTimer >= 0.0) {
            // Cranking. The car stays put until the starter sample has run, the
            // same order CarScene uses for the player reversing out - a car
            // that pulls away silently and then starts its engine is backwards.
            exitCrankTimer += deltaTime;
            if (exitCrankTimer >= CalculateSampleDuration(SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE)) {
                exitCrankTimer = -1.0;
                exitingCar = true;
            }
        } else {
            debugWaitTimer += deltaTime;

            // Hold the timer while a challenge dialog is in progress
            if (!CanStartCarExit()) {
                debugWaitTimer = 0;
            }
            // Fix: Manually trigger exit after 3 seconds if base class doesn't do it
            else if (debugWaitTimer >= 3.0) {
                LOG_DEBUG("Diner::UpdateCarAnimation - Manual exit trigger after 3 seconds");
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::ENGINE_START, 0.6f);
                }
                exitCrankTimer = 0.0;
                debugWaitTimer = 0;  // Reset for next cycle
            }
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

    if (hoodView) {
        SetActiveDialog(nullptr);
        hoodView.reset();
    }
    hoodPopped = false;
    hoodTimer = 0.0f;

    challenge->Reset();

    // Reset the actors; both start off screen.
    waitressStep = -1;
    waitressOnScreen = false;
    waitressTurnAround = false;
    waitressWaits = 0;
    guyStep = -1;
    dinerRollTimer = 0.0f;
    if (guyAnim[0]) { ApplyWaitressFrame(); ApplyGuyFrame(); }

    // Call parent CarScene implementation (which handles common animation state)
    CarScene::OnSceneExit();
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
    buttons[1].cancelChoice = true;

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
