#include "core/game_text.h"
#include "core/log.h"
#include "scenes/diner.h"
#include "core/state.h"
#include "ui/hud.h"
#include "car/cardata.h"
#include "debug/debug_overlay.h"
#include "gfx/animated_bitmap.h"
#include "scenes/scene_layout.h"
#include "embedded_resources.h"
#include "ui/main_font.h"
#include "core/utils.h"
#include "scenes/diner_actors.h"
#include "car/engine_bay.h"
#include "race/roster.h"
#include <cstdlib>
#include <algorithm>
#include <random>

double carAnimationTimeDiner = 0;
double carExitAnimationTime = 0;

namespace {
Asset DinerBackground() {
    if (state.GetAssets().GetGame() == SrGame::SR2)
        return (Asset){LibAssetRegular, SrLibFileAmigaLibP, SR2_DRIVE_IN_SCREEN, SrGame::SR2};
    return (Asset){LibAssetRegular, SrLibFileAmigaLibP, DINER_SCREEN};
}
}

Diner::Diner() : CarScene(DinerBackground()) {
    lastFrameTime = GameTicks() / 1000.0;
    carBitmap = nullptr;
    isPaused = false;
    challenge = std::make_unique<Challenge>();

    zones = SceneLayout::Load(state.GetAssets().GetGame() == SrGame::SR2 ? embedded_diner_sr2_toml
                                                                          : embedded_diner_toml);

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
        if (!OpponentPresent()) return;
        // One look per challenger; a second try just annoys him.
        if (hoodPopped) { challenge->Taunt(); return; }
        hoodPopped = true;
        hoodTimer = 0.0f;
        // Rebuilt per opponent: the parts drawn are baked in at construction.
        hoodView = std::make_unique<EngineBayView>(*currentOpponent->car, state,
                                                   [this] { CloseHoodView(); });
        SetActiveDialog(hoodView->GetDialog());
    };
    GetZone("challenge_opponent").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        if (!OpponentPresent()) return;
        // Who is on screen, not the last pool index, which a phoned or re-picked opponent moves. The King never
        // gets the bet dialog.
        if (state.IsKingOpponent(currentOpponent)) {
            challenge->OpenKing(state.GetAssets().GetMainFont());
        } else {
            challenge->Open(state.GetAssets().GetMainFont());
        }
    };
}

void Diner::Init(bool debugMode) {
    CarScene::Init(debugMode);

    SetAudioManager(state.GetAssets().GetAudioManager());

    // The strip is the road edge the car sits behind. SR2's is the rail, from y=162 to the bottom of the art.
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        roadMaskSrcRect = {0, 162, 320, 28};
        roadMaskDestRect = {0, 0, 320, 28};
    }
    roadMaskSurface.reset(SDL_CreateRGBSurface(0, roadMaskSrcRect.w, roadMaskSrcRect.h, 32, 0, 0, 0, 0));
    SDL_BlitSurface(background->GetSurface(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
    roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
    SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);

    auto guyBubble = std::make_unique<Bubble>(GameText::Msg(GameText::WHATS_HIS_NAME), BUBBLE_RIGHT, true);
    guyBubble->SetVisible(false);
    bubbles.push_back(std::move(guyBubble));

    // Street Rod 2's drive-in has no waitress, phone guy, screen mask or jail overlay on the Amiga.
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    if (!sr2) {
        LoadActors();

        jailBars = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, JAIL});
        jailBars->SetPosition(0, 0);
        jailBars->SetVisible(true);
    }

    Bitmap* screenMaskBitmap =
        sr2 ? nullptr : state.GetAssets().GetBitmapAsset({LibAssetMask, SrLibFileAmigaLibP, DINER_SCREEN_MASK});
    if (screenMaskBitmap) {
        maskedScreenBackground.reset(SDL_CreateRGBSurfaceWithFormat(0, 256, 41, 32, SDL_PIXELFORMAT_RGBA8888));
        background->CreateMask(screenMaskBitmap->GetSurface(), maskedScreenBackground.get(), {0, 68, 256, 41});
        maskedScreenBackgroundTexture.reset(SDL_CreateTextureFromSurface(renderer, maskedScreenBackground.get()));
        SDL_SetTextureBlendMode(maskedScreenBackgroundTexture.get(), SDL_BLENDMODE_BLEND);
    }

    hud->SetVisible(true);

    initStartTime = GameTicks();
}

Diner::~Diner() {
    opponentCarBitmaps.clear();
    carBitmap = nullptr;
}

SDL_Point Diner::GetCarIdlePosition() {
    if (!currentOpponent) return {80, 95};
    int x = SrLibQueryGetCarIdleX(SCENE_DINER);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_DINER, currentOpponent->car->GetModel().height,
                                       state.GetAssets().GetGame());
    return {x, y};
}


using namespace srdiner;

void Diner::LoadActors() {
    if (guyAnim[0]) return;

    const AmigaLibAsset guySheets[2] = {PHONEBOOK_GUY_1, PHONEBOOK_GUY};
    for (int s = 0; s < 2; ++s) {
        Bitmap* sheet = state.GetAssets().GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, guySheets[s]});
        guyAnim[s] = std::make_unique<AnimatedBitmap>(sheet, SDL_Rect{kGuyX, kGuyBottomY - kGuyRowH[0], kGuyFrameW, kGuyRowH[0]});
        // The two sheets differ in width (192 and 200), so clip the fixed 40-wide cell rather than stretch it.
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
        // 11 frames need two rows; the twelfth cell is blank on both sheets.
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
    waitressPauseTimer = -1.0f;
    waitressPauseDone = (dir != 0) || !state.GetAssets().GetExtraAnimationsRef();
    ApplyWaitressFrame();
}

void Diner::ApplyWaitressFrame() {
    if (waitressStep < 0) {
        for (int d = 0; d < 2; ++d) waitressAnim[d]->SetVisible(false);
        return;
    }
    // Standing borrows 335's upright pose whichever way she was headed.
    const bool standing = waitressPauseTimer >= 0.0f;
    const int sheet = standing ? kWaitressStandSheet : waitressDir;
    const int frame = standing ? kWaitressStandFrame : kWaitressFrames[waitressStep];
    for (int d = 0; d < 2; ++d) waitressAnim[d]->SetVisible(d == sheet);
    waitressAnim[sheet]->SetFrame(frame - 1);
    SDL_Rect r = waitressAnim[sheet]->GetPosition();
    waitressAnim[sheet]->SetPosition(WaitressX(waitressDir, waitressStep), kWaitressY);
    waitressAnim[sheet]->SetDimensions(r.w, r.h);
}

void Diner::ApplyGuyFrame() {
    const int frame = guyStep >= 0 ? kGuyFrames[guyStep] : 0;
    const int sheet = frame / 10;
    for (int s = 0; s < 2; ++s) guyAnim[s]->SetVisible(s == sheet);
    guyAnim[sheet]->SetFrame(frame % 10);
    // At its own size, bottom-aligned, so a clipped cell sits lower rather than scaling up.
    const SDL_Rect src = guyAnim[sheet]->GetCurrentFrameRect();
    guyAnim[sheet]->SetPosition(kGuyX, kGuyBottomY - src.h + 1);
    guyAnim[sheet]->SetDimensions(src.w, src.h);
}

void Diner::UpdateActors(double deltaTime) {
    if (!guyAnim[0]) return;
    const float dtMs = static_cast<float>(deltaTime) * 1000.0f;

    if (guyStep >= 0) {
        guyHoldTimer += dtMs;
        const float hold = kGuyHold[kGuyFrames[guyStep]] * GUY_STEP_MS;
        if (guyHoldTimer >= hold) {
            guyHoldTimer -= hold;
            if (++guyStep >= kGuySteps) guyStep = -1;
        }
        ApplyGuyFrame();
    }

    if (waitressStep >= 0) {
        if (waitressPauseTimer >= 0.0f) {
            waitressPauseTimer += dtMs;
            if (waitressPauseTimer >= kWaitressPauseMs) waitressPauseTimer = -1.0f;
            ApplyWaitressFrame();
            return;
        }
        if (!waitressPauseDone && waitressStep >= WaitressPauseStep(kWaitressPauseX)) {
            waitressPauseDone = true;
            waitressPauseTimer = 0.0f;
            ApplyWaitressFrame();
            return;
        }
        waitressStepTimer += dtMs;
        while (waitressStepTimer >= WAITRESS_STEP_MS) {
            waitressStepTimer -= WAITRESS_STEP_MS;
            if (++waitressStep >= kWaitressSteps) {
                waitressStep = -1;
                waitressOnScreen = (waitressDir == 0);
                if (waitressTurnAround) {
                    waitressTurnAround = false;
                    StartWaitressWalk(1);
                }
                break;
            }
        }
        ApplyWaitressFrame();
        if (waitressStep < 0 && waitressOnScreen) {
            waitressAnim[0]->SetVisible(true);
        }
    }

    if (waitressStep >= 0 || guyStep >= 0) return;

    // The percentages are the original's; the cadence is not.
    dinerRollTimer += static_cast<float>(deltaTime);
    if (dinerRollTimer < ROLL_SECONDS) return;
    dinerRollTimer = 0.0f;

    std::uniform_int_distribution<int> roll100(0, 99);
    // The original's jail loop never reaches her scheduler. The phone guy still shows.
    if (state.IsJailed()) {
        if (roll100(RandomEngine()) < 6) {
            guyStep = 0;
            guyHoldTimer = 0.0f;
            ApplyGuyFrame();
        }
        return;
    }
    if (!waitressOnScreen) {
        // Odds climb the longer she has been away, and she never comes straight back.
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
    hud->SetVisible(!state.IsJailed());

    currentTime = GameTicks() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    // With no car there is nothing to challenge or look under, so these zones stay off.
    SetZoneEnabled("pop_hood", OpponentPresent());
    SetZoneEnabled("challenge_opponent", OpponentPresent());

    UpdateActors(deltaTime);
    if (carBitmap && state.GetAssets().GetGame() == SrGame::SR2) carBitmap->SetShadowVisible(false);

    // He shuts the hood himself after a while - no studying it at leisure.
    if (IsHoodViewOpen()) {
        hoodTimer += static_cast<float>(deltaTime);
        if (hoodTimer >= HOOD_SECONDS) CloseHoodView();
    }

    AnimatedBitmap* waitressLeft = waitressAnim[1].get();
    AnimatedBitmap* waitressRight = waitressAnim[0].get();

    Scene::DrawScene();
    if (!isSetup) {
        Init(debugMode);
        isSetup = true;
    }

    if (initialDelay) {
        if (GameTicks() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
        }

        if (waitressLeft) {
            waitressLeft->Draw();
        }
        if (waitressRight) {
            waitressRight->Draw();
        }

        if (maskedScreenBackgroundTexture) {
            SDL_Rect screenMaskRect = {0, 68, 256, 41};
            SDL_RenderCopy(renderer, maskedScreenBackgroundTexture.get(), NULL, &screenMaskRect);
        }

        for (auto& g : guyAnim) if (g) g->Draw();

        for (auto& b : bubbles) {
            b->Draw();
        }
        if (debugMode && debugOverlay) {
            DrawLegacyDebugOverlay();
        }
        return;
    }

    if (waitressLeft) {
        waitressLeft->Draw();
    }
    if (waitressRight) {
        waitressRight->Draw();
    }

    if (maskedScreenBackgroundTexture) {
        SDL_Rect screenMaskRect = {0, 68, 256, 41};
        SDL_RenderCopy(renderer, maskedScreenBackgroundTexture.get(), NULL, &screenMaskRect);
    }

    for (auto& g : guyAnim) if (g) g->Draw();

    if (carBitmap != nullptr) {
        carBitmap->Move({carx, cary});
        carBitmap->Draw();
    }

    // The hood window reaches into the road strip, so DrawMaskAfterCar draws it after the mask.
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
        challenge->Reset();
        StartOpponentExit();
    }

    if (debugMode && debugOverlay) {
        DrawLegacyDebugOverlay();
    }
    hud->Draw();

}

void Diner::DrawMaskAfterCar() {
    if (roadMaskSurface && roadMaskTexture) {
        SDL_BlitSurface(surface.get(), &roadMaskSrcRect, roadMaskSurface.get(), &roadMaskDestRect);
        roadMaskTexture.reset(SDL_CreateTextureFromSurface(renderer, roadMaskSurface.get()));
        SDL_SetTextureBlendMode(roadMaskTexture.get(), SDL_BLENDMODE_NONE);
        SDL_RenderCopy(renderer, roadMaskTexture.get(), NULL, &roadMaskSrcRect);
    }

    CarScene::DrawMaskAfterCar();

    if (IsHoodViewOpen()) hoodView->GetDialog()->Draw();

    // Last, from DrawExtra, so nothing draws over the bars.
    if (state.IsJailed() && jailBars) jailBars->Draw();
}

bool Diner::IsHoodViewOpen() const {
    return hoodView && hoodView->GetDialog()->IsVisible();
}

// Not hoodView.reset(): this runs from the view's own dialog callback.
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

    // Every challenger that drives in comes through here, which keeps the pool, tier and King unlock in play.
    state.PickNextOpponent();

    this->currentOpponent = state.GetNextOpponent();
    // Everything below reads currentOpponent->car without checking.
    if (!currentOpponent || !currentOpponent->car) {
        currentOpponent = nullptr;
        return;
    }

    state.AddKnownOpponent(currentOpponent);
    kingDriveBy = state.IsKingOpponent(currentOpponent) && state.KingOnlyDrivesBy();

    std::string oppoText = GetOpponentText(currentOpponent);

    hud->SetText(oppoText.c_str());
    SetZoneText("background", oppoText);

    // Keyed on the car, not the driver's name: pool slots share Opponents and a lost pink slip replaces the car.
    auto existingBitmap = opponentCarBitmaps.find(currentOpponent->car);
    if (existingBitmap != opponentCarBitmaps.end()) {
        carBitmap = existingBitmap->second.get();
    } else {
        StandardCarColor carColor = static_cast<StandardCarColor>(currentOpponent->car->GetColor());
        auto newBitmap = std::make_unique<CarBitmap>(currentOpponent->car, &currentOpponent->driver, carColor);
        carBitmap = newBitmap.get();
        opponentCarBitmaps[currentOpponent->car] = std::move(newBitmap);
    }

    int carYOffset = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(),
                                                  currentOpponent->car->GetModel().height,
                                                  state.GetAssets().GetGame());
    this->cary = carYOffset;

    // Sprite sizes differ per model, so anchor on the air-scoop offset at the idle position.
    const SDL_Point idle = GetCarIdlePosition();
    const BitmapOffset_t hoodOffsets = currentOpponent->car->GetModel().offsets;
    SetZoneRect("pop_hood", {idle.x + hoodOffsets.offsetXAirScoop - 24,
                             idle.y + hoodOffsets.offsetYAirScoop - 4, 64, 26});

    hoodPopped = false;
    exitingCar = false;
    debugWaitTimer = 0;
    exitCrankTimer = -1.0;
    engineSoundStarted = false;
    LOG_INFO("Diner::StartAnimateCar - Reset state: exitingCar=0, debugWaitTimer=0, engineSoundStarted=0");

    CarScene::StartAnimateCar(currentOpponent->car, &currentOpponent->driver, CarAnimationType::OPPONENT_CONTINUOUS);
}

void Diner::UpdateDebugInfo() {
    if (!debugMode) return;

    // The debug UI reads this without debugOverlay; the base class rows are kept.
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
    // In jail nothing is clickable; a key ends the run.
    if (state.IsJailed()) {
        if (event->type == SDL_KEYDOWN) {
            // MoveToScene reads the flag to build the hall of fame as a loss; clear it after or the diner stays locked.
            // ponytail: a proper end-of-run reset belongs to the game-over sequence.
            state.MoveToScene(SCENE_KING_OF_COUNTY);
            state.SetJailed(false);
        }
        return true;
    }

    if (challenge->HandleEvent(event)) return true;

    // A stray click must not walk out to the garage on a race that is already happening.
    const bool dialogUp = activeDialog && activeDialog->IsVisible();
    if (challenge->IsActive() && !dialogUp &&
        (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)) {
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
    OnSceneExit();

    Scene::FadeOut();
}

void Diner::UpdateCarAnimation(double deltaTime) {
    static bool lastAnimatingCar = false;
    static bool lastExitingCar = false;
    
    CarScene::UpdateCarAnimation(deltaTime);

    if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && !exitingCar && carBitmap) {
        if (exitCrankTimer >= 0.0) {
            // The car holds still until the starter sample has run.
            exitCrankTimer += deltaTime;
            if (exitCrankTimer >= CalculateSampleDuration(SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE)) {
                exitCrankTimer = -1.0;
                exitingCar = true;
            }
        } else {
            debugWaitTimer += deltaTime;

            if (!CanStartCarExit()) {
                debugWaitTimer = 0;
            }
            else if (debugWaitTimer >= 3.0) {
                LOG_DEBUG("Diner::UpdateCarAnimation - Manual exit trigger after 3 seconds");
                StartOpponentExit();
            }
        }
    } else if (currentAnimationType == CarAnimationType::OPPONENT_CONTINUOUS && !animatingCar && exitingCar && carBitmap) {
        static double exitLogTimer = 0;
        exitLogTimer += deltaTime;
        if (exitLogTimer >= 1.0) {
            LOG_DEBUG("Diner::UpdateCarAnimation - In exit state: exitingCar=%d, animatingCar=%d", exitingCar, animatingCar);
            exitLogTimer = 0;
        }
    }

    if (exitingCar != lastExitingCar) {
        LOG_DEBUG("Diner::UpdateCarAnimation - exitingCar changed: %d -> %d", lastExitingCar, exitingCar);
        lastExitingCar = exitingCar;
    }
    if (animatingCar != lastAnimatingCar) {
        LOG_DEBUG("Diner::UpdateCarAnimation - animatingCar changed: %d -> %d", lastAnimatingCar, animatingCar);
        lastAnimatingCar = animatingCar;
    }

    if (exitingCar && !engineSoundStarted) {
     
        if (audioManager) {
            lastAnimationEnginePeriod = -1;
            ApplyAnimationEngine(false, false);
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
            engineSoundStarted = true;
        }
    }

    if (!exitingCar && engineSoundStarted) {
        engineSoundStarted = false;
        LOG_DEBUG("Diner::UpdateCarAnimation - Exit animation completed, resetting engine sound flag");
    }

}

void Diner::StartOpponentExit() {
    if (!carBitmap || exitingCar || exitCrankTimer >= 0.0) return;
    if (audioManager) {
        audioManager->PlaySound(AudioSampleType::ENGINE_START, 0.6f);
    }
    exitCrankTimer = 0.0;
    debugWaitTimer = 0;
}

void Diner::OnCarAnimationComplete() {
    LOG_INFO("Diner::OnCarAnimationComplete - Opponent car reached idle position, stopping drive-in engine sound");

    // Animation mode 3: the drive-out follows at once, engine still running.
    if (kingDriveBy) {
        if (audioManager) {
            lastAnimationEnginePeriod = -1;
            ApplyAnimationEngine(false, false);
            audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
        }
        engineSoundStarted = true;
        TriggerCarExit();
        return;
    }

    if (audioManager) {
        audioManager->StopLoop(0);
        LOG_INFO("Diner::OnCarAnimationComplete - Drive-in engine sound stopped");
    }
}

void Diner::OnCarExitComplete() {
    LOG_INFO("Diner::OnCarExitComplete - Opponent car exit animation completed");
    // The driver and car are gone; nothing to describe until the next one pulls in.
    hud->SetText("");
    SetZoneText("background", "");
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

    waitressStep = -1;
    waitressOnScreen = false;
    waitressTurnAround = false;
    waitressWaits = 0;
    guyStep = -1;
    dinerRollTimer = 0.0f;
    if (guyAnim[0]) { ApplyWaitressFrame(); ApplyGuyFrame(); }

    CarScene::OnSceneExit();
}

void Diner::ShowCallListDialog() {
    state.OnPhoneListOpened();
    state.AdvanceTicks(150);   // ldrivein.c 0x25108a
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
            case 0: {
                int selectedIndex = callListDialog->GetSelectedIndex();
                if (selectedIndex >= 0) {
                    const auto& knownOpps = state.GetKnownOpponents();
                    if (selectedIndex < (int)knownOpps.size()) {
                        Opponent* selected = knownOpps[selectedIndex];
                        state.SetNextOpponent(selected);
                        callListDialog->Hide();
                        SetActiveDialog(nullptr);
                        if (carBitmap && !exitingCar) {
                            exitingCar = true;
                            debugWaitTimer = 0;
                        }
                    }
                }
                break;
            }
            case 1:
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
        fadeCompleteTime = GameTicks();
    } else {
        if (!animatingCar && !exitingCar && !carBitmap) {
            StartAnimateCar(state.GetCurrentCarPtr(), nullptr, CarAnimationType::OPPONENT_CONTINUOUS);
        }
    }
}
