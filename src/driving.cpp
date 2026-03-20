#include "log.h"
#include "driving.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "reputation.h"
#include <fstream>
#include <vector>
#include <cstring>

Driving::Driving() : Scene() {
    LOG_INFO("Driving::Driving() constructor called\n");
    // Create racing component with audio manager
    racing = std::make_unique<Racing>(state.GetAssets().GetAudioManager());
    roadTexture = nullptr;
    lastEvent = nullptr;
    
    // Initialize track reader
    trackReader = std::make_unique<TrackReader>();
    LOG_INFO("Driving constructor completed - trackReader created\n");
}

void Driving::Init(bool debugMode) {
    LOG_INFO("Driving::Init() called with debugMode=%s\n", debugMode ? "true" : "false");
    Scene::Init(debugMode);
    
    
    auto dashboard = [&]() {        
        StandardCarColor carColor = state.GetCurrentCar()->GetColor();
        std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
        byte_vector& libFile = state.GetAssets().GetLibPData();
        return state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_INTERIOR_1}, carPalette);    
    }();
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);
    AddDrawable("dashboard", dashboard);

    // Load pillar (windshield beam)
    auto pillar = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WINDSHIELD_BEAM});
    pillar->AlignAbove(dashboard);
    pillar->SetVisible(true);
    AddDrawable("pillar", pillar);

    // Load roof (ceiling)
    auto roof = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CEILING});
    roof->AlignAbove(pillar);
    roof->SetVisible(true);
    AddDrawable("roof", roof);

    // Load hood with current car's color
    auto hood = [&]() {        
        StandardCarColor carColor = state.GetCurrentCar()->GetColor();
        std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
        byte_vector& libFile = state.GetAssets().GetLibPData();
        return state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD}, carPalette);    
    }();
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    AddDrawable("hood", hood);

    // Load mirror stem
    // auto mirrorStem = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_THING3});
    // mirrorStem->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    // mirrorStem->SetVisible(true);
    // AddDrawable("mirror_stem", mirrorStem);

    // Load mirror
    auto mirror = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_REARVIEWMIRROR_FRAME});
    mirror->SetPosition(240, 19);
    mirror->SetVisible(true);
    AddDrawable("mirror", mirror);
    
    // Load clock for automatic transmission cars (will overlap tachometer)
    auto clock = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_CLOCK_11_50});
    clock->SetPosition(120, 115);
    clock->SetVisible(false); // Will be shown only for automatic transmission
    AddDrawable("clock", clock);

    // Load plush dice
    auto plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE1});
    plushDice->SetPosition(259, 45);
    plushDice->SetVisible(true);
    AddDrawable("plush_dice1", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE2});
    plushDice->SetPosition(253, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice2", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE3});
    plushDice->SetPosition(250, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice3", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE4});
    plushDice->SetPosition(267, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice4", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE5});
    plushDice->SetPosition(274, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice5", plushDice);

    // Load wheel left hand 1
    wheelLeftHand1 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_1});
    wheelLeftHand1->SetPosition(72, 144);
    wheelLeftHand1->SetVisible(true);
    AddWheelDrawable("wheel_left_hand_1", wheelLeftHand1);

    wheelLeftHand2 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_2});
    wheelLeftHand2->SetPosition(72, 120);
    wheelLeftHand2->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_2", wheelLeftHand2);

    wheelLeftHand3 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_3});
    wheelLeftHand3->SetPosition(24, 144);
    wheelLeftHand3->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_3", wheelLeftHand3);

    wheelLeftHand4 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_4});
    wheelLeftHand4->SetPosition(72, 144);
    wheelLeftHand4->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_4", wheelLeftHand4);

    // Load wheel right hand 1
    wheelRightHand1 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_1});
    wheelRightHand1->SetPosition(216, 144);
    wheelRightHand1->SetVisible(true);
    AddWheelDrawable("wheel_right_hand_1", wheelRightHand1);

    // Load wheel right hand 2
    wheelRightHand2 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_2});
    wheelRightHand2->SetPosition(216, 120);
    wheelRightHand2->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_2", wheelRightHand2);

    // Load wheel right hand 3
    wheelRightHand3 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_3});
    wheelRightHand3->SetPosition(216, 144);
    wheelRightHand3->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_3", wheelRightHand3);

    // Load wheel right hand 4
    wheelRightHand4 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_4});
    wheelRightHand4->SetPosition(272, 144);
    wheelRightHand4->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_4", wheelRightHand4);

    wheelTurningLeft = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT});
    wheelTurningLeft->SetPosition(120, 144);
    wheelTurningLeft->SetVisible(false);
    AddWheelDrawable("wheel_turning_left", wheelTurningLeft);

    wheelTurningRight = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT});
    wheelTurningRight->SetPosition(120, 144);
    wheelTurningRight->SetVisible(false);
    AddWheelDrawable("wheel_turning_right", wheelTurningRight);

    wheelTurningLeftOpposite = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT_OPPOSITE_1});
    wheelTurningLeftOpposite->SetPosition(72, 144);
    wheelTurningLeftOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_left_opposite", wheelTurningLeftOpposite);

    wheelTurningRightOpposite = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_1});
    wheelTurningRightOpposite->SetPosition(216, 144);
    wheelTurningRightOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_right_opposite", wheelTurningRightOpposite);

    SetState(DRIVING_STATE_NORMAL);
    
    // Load crash overlay bitmap with current car's color palette
    StandardCarColor carColor = state.GetCurrentCar()->GetColor();
    std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
    byte_vector& libFile = state.GetAssets().GetLibPData();
    auto crashOverlayAsset = state.GetAssets().GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_CRASHED_INTERIOR}, carPalette);
    if (crashOverlayAsset) {
        crashOverlayBitmap = std::make_unique<Bitmap>(*crashOverlayAsset);
        crashOverlayBitmap->SetVisible(false); // Initially hidden
        crashOverlayBitmap->SetPosition(0, 18);
    }
    
    // Load track data for debugging/development
    LoadTrackData();
}

void Driving::LoadTrackData() {
    LOG_INFO("Driving::LoadTrackData() called\n");
    
    // Check if trackReader exists
    if (!trackReader) {
        LOG_INFO("ERROR: trackReader is null!\n");
        return;
    }
    
    // Check if AssetManager has executable data (where track data is stored)
    byte_vector& srExeData = state.GetAssets().GetSrExeData();
    LOG_INFO("AssetManager SrExeData size: %zu bytes\n", srExeData.size());
    
    if (srExeData.empty()) {
        LOG_INFO("ERROR: AssetManager SrExeData is empty!\n");
        return;
    }
    
    // Load Street Rod executable data from AssetManager
    if (trackReader->LoadTrackData(srExeData)) {
        LOG_INFO("=== Street Rod Track Data Loaded ===\n");
        
        // Load road race track
        LOG_INFO("Extracting road race track coordinates...\n");
        auto roadRaceSegments = trackReader->ExtractTrackCoordinates(SR_TRACK_ROAD_RACE, 500);
        LOG_INFO("Found %zu road race segments\n", roadRaceSegments.size());
        if (!roadRaceSegments.empty()) {
            auto analysis = trackReader->AnalyzeTrack(roadRaceSegments);
            LOG_INFO("Road Race Track:\n");
            LOG_INFO("  Segments: %d\n", analysis.segmentCount);
            LOG_INFO("  Curved: %d, Straight: %d\n", analysis.curvedSegments, analysis.straightSegments);
            LOG_INFO("  Length: %.1f units\n", analysis.totalLength);
            LOG_INFO("  Bounds: (%d,%d) to (%d,%d)\n", analysis.minX, analysis.minY, analysis.maxX, analysis.maxY);
            LOG_INFO("  Unique sprites: %zu\n", analysis.uniqueSprites.size());
        }
        
        // Load drag race track
        LOG_INFO("Extracting drag race track coordinates...\n");
        auto dragRaceSegments = trackReader->ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 200);
        LOG_INFO("Found %zu drag race segments\n", dragRaceSegments.size());
        if (!dragRaceSegments.empty()) {
            auto analysis = trackReader->AnalyzeTrack(dragRaceSegments);
            LOG_INFO("Drag Race Track:\n");
            LOG_INFO("  Segments: %d\n", analysis.segmentCount);
            LOG_INFO("  Curved: %d, Straight: %d\n", analysis.curvedSegments, analysis.straightSegments);
            LOG_INFO("  Length: %.1f units\n", analysis.totalLength);
            LOG_INFO("  Bounds: (%d,%d) to (%d,%d)\n", analysis.minX, analysis.minY, analysis.maxX, analysis.maxY);
        }
        
        // Load roadside sprites
        LOG_INFO("Extracting roadside sprites...\n");
        auto roadsideSprites = trackReader->ExtractRoadSideSprites(SR_ROADSIDE_SPRITES_OFFSET, 100);
        LOG_INFO("Found %zu roadside sprite records\n", roadsideSprites.size());
        if (!roadsideSprites.empty()) {
            LOG_INFO("Roadside Sprites: %zu records\n", roadsideSprites.size());
            
            // Show first few sprites for debugging
            for (size_t i = 0; i < std::min<size_t>(5, roadsideSprites.size()); ++i) {
                const auto& sprite = roadsideSprites[i];
                LOG_INFO("  %s at pos %d, %s at pos %d\n",
                    TrackReader::GetSpriteName(sprite.spriteId1), sprite.trackPosition1,
                    TrackReader::GetSpriteName(sprite.spriteId2), sprite.trackPosition2);
            }
        }
        
        // Load track data into racing component
        if (racing) {
            LOG_INFO("Loading track data into racing component...\n");
            racing->LoadTrackData(srExeData, SR_TRACK_ROAD_RACE);
            
            // Configure racing physics based on current car's transmission
            Car* currentCar = state.GetCurrentCar();
            if (currentCar) {
                // Automatic transmissions are types 0-2 (SR_TRANS_AUTO_GM, SR_TRANS_AUTO_FORD, SR_TRANS_AUTO_CHRYSLER)
                bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                racing->SetTransmissionType(isAutomatic);
                racing->SetTransmissionType(false);
                LOG_INFO("Car transmission: %s\n", isAutomatic ? "Automatic" : "Manual");
            }
            
            // Initialize opponent for the race
            LOG_INFO("Initializing race opponent...\n");
            racing->InitializeOpponent();
        }
        
        LOG_INFO("================================\n");
    } else {
        LOG_INFO("Failed to load Street Rod track data from AssetManager executable data.\n");
        LOG_INFO("Make sure the Street Rod executable is properly loaded.\n");
    }
}

Driving::~Driving() {
    // Timer no longer used
    // SDL_RemoveTimer(timerID);
    
    // Clean up road texture
    if (roadTexture) {
        SDL_DestroyTexture(roadTexture);
        roadTexture = nullptr;
    }
}

void Driving::AddWheelDrawable(const std::string& id, Bitmap* bitmap) {
    AddDrawable(id, bitmap);
}

void Driving::Start() {
    // Timer no longer needed as racing is updated in Draw() method
    // timerID = SDL_AddTimer(250, AnimationTimerCallback, this);
}

void Driving::SetState(DrivingState state) {
    // Hide all wheel elements first
    wheelLeftHand1->SetVisible(false);
    wheelLeftHand2->SetVisible(false);
    wheelLeftHand3->SetVisible(false);
    wheelLeftHand4->SetVisible(false);
    wheelRightHand1->SetVisible(false);
    wheelRightHand2->SetVisible(false);
    wheelRightHand3->SetVisible(false);
    wheelRightHand4->SetVisible(false);
    wheelTurningLeft->SetVisible(false);
    wheelTurningRight->SetVisible(false);
    wheelTurningLeftOpposite->SetVisible(false);
    wheelTurningRightOpposite->SetVisible(false);

    this->currentState = state;
    switch (state) {
        case DRIVING_STATE_NORMAL:
            // Show default hand positions
            wheelLeftHand1->SetVisible(true);
            wheelRightHand1->SetVisible(true);
            break;
        case DRIVING_STATE_SHIFTING:
            break;
        case DRIVING_STATE_TURNING_LEFT:
            wheelRightHand2->SetVisible(true);
            wheelRightHand3->SetVisible(true);
            wheelRightHand4->SetVisible(true);
            wheelTurningLeft->SetVisible(true);
            wheelTurningLeftOpposite->SetVisible(true);
            break;
        case DRIVING_STATE_TURNING_RIGHT:
            wheelLeftHand2->SetVisible(true);
            wheelLeftHand3->SetVisible(true);
            wheelLeftHand4->SetVisible(true);
            wheelTurningRight->SetVisible(true);
            wheelTurningRightOpposite->SetVisible(true);
            break;
        case DRIVING_STATE_CRASHED:
            // All wheels are already hidden, do nothing
            break;
    }
}

void Driving::UpdateDiceAnimation(float deltaTime, bool carIsMoving) {
    if (!carIsMoving) {
        // Reset timer when car is not moving - dice stop
        diceAnimationTimer = 0.0f;
        
        // Reset to dice 1 (neutral position) when car stops
        if (currentDiceIndex != 0) {
            // Hide current dice
            std::string currentDiceName = "plush_dice" + std::to_string(currentDiceIndex + 1);
            auto currentDiceDrawable = GetDrawable(currentDiceName);
            if (currentDiceDrawable) {
                currentDiceDrawable->SetVisible(false);
            }
            
            // Set to dice 1 (index 0) and show it
            currentDiceIndex = 0;
            diceAnimationForward = true; // Reset direction to forward for next movement
            
            std::string neutralDiceName = "plush_dice1";
            auto neutralDiceDrawable = GetDrawable(neutralDiceName);
            if (neutralDiceDrawable) {
                neutralDiceDrawable->SetVisible(true);
            }
        }
        return;
    }
    
    // Update animation timer only when car is moving
    diceAnimationTimer += deltaTime;
    
    // Check if it's time to change dice
    if (diceAnimationTimer >= DICE_ANIMATION_INTERVAL) {
        diceAnimationTimer = 0.0f; // Reset timer
        
        // Hide current dice
        std::string currentDiceName = "plush_dice" + std::to_string(currentDiceIndex + 1);
        auto currentDiceDrawable = GetDrawable(currentDiceName);
        if (currentDiceDrawable) {
            currentDiceDrawable->SetVisible(false);
        }
        
        // Move to next dice with bidirectional cycling
        if (diceAnimationForward) {
            currentDiceIndex++;
            if (currentDiceIndex >= 4) { // Reached dice 5 (index 4)
                diceAnimationForward = false; // Start going backward
            }
        } else {
            currentDiceIndex--;
            if (currentDiceIndex <= 0) { // Reached dice 1 (index 0)
                diceAnimationForward = true; // Start going forward again
            }
        }
        
        // Show new dice
        std::string newDiceName = "plush_dice" + std::to_string(currentDiceIndex + 1);
        auto newDiceDrawable = GetDrawable(newDiceName);
        if (newDiceDrawable) {
            newDiceDrawable->SetVisible(true);
        }
    }
}

void Driving::Draw() {
    Scene::Draw();

    // Update racing simulation
    if (racing) {
        // Get frame delta time in seconds, with a reasonable maximum to prevent huge jumps
        float deltaTime = state.GetElapsedTime() / 1000.0f;
        if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap at 100ms to prevent huge jumps
        
        racing->Draw(deltaTime, lastEvent);

        // Check race outcome.
        RaceOutcome outcome = racing->GetRaceOutcome();
        if (outcome != RaceOutcome::Ongoing) {
            bool won = (outcome == RaceOutcome::PlayerWon);
            RaceType raceType = (racing->GetTrackType() == SR_TRACK_DRAG_RACE)
                                ? RaceType::Drag : RaceType::Road;
            bool endGame = state.OnRaceFinished(raceType, won);
            racing->ResetRaceOutcome();
            if (endGame) {
                LOG_INFO("Driving: end-game triggered (King beaten in drag)");
                // TODO: show end-game cutscene; for now return to diner.
            }
            state.MoveToScene(SCENE_DINER);
        }

        // Update dice animation based on car movement
        bool carIsMoving = racing->GetSpeed() > 0.1f; // Consider car moving if speed > 0.1
        UpdateDiceAnimation(deltaTime, carIsMoving);
        
        // Get the updated racing surface and create texture
        SDL_Surface* racingSurface = racing->GetSurface();
        if (racingSurface) {
            // Destroy old texture if it exists
            if (roadTexture) {
                SDL_DestroyTexture(roadTexture);
            }
            
            // Create new texture from racing surface
            roadTexture = SDL_CreateTextureFromSurface(renderer, racingSurface);
            if (roadTexture) {
                SDL_RenderCopy(renderer, roadTexture, NULL, &roadRect);
            }
        }
    }

    // Update clock visibility based on transmission type
    bool isAutomatic = racing && racing->IsAutomatic();
    
    auto clockDrawable = GetDrawable("clock");
    if (clockDrawable) {
        clockDrawable->SetVisible(isAutomatic);
    }
    
    // Draw all interior elements using drawable system
    DrawAll();
    
    // Draw gauge needles on top of dashboard
    if (racing) {
        // Only draw tachometer for manual transmission cars
        if (!isAutomatic) {
            racing->DrawTachometerNeedleToRenderer(renderer);
        }
        racing->DrawSpeedometerNeedleToRenderer(renderer);
    }
    
    // Handle crash state - set proper driving state
    bool isCrashed = racing && racing->ShouldShowCrashOverlay();
    
    // Only change state when crash state changes
    if (isCrashed != lastCrashState) {
        if (isCrashed) {
            SetState(DRIVING_STATE_CRASHED);
        } else {
            SetState(DRIVING_STATE_NORMAL);
        }
        lastCrashState = isCrashed;
    }
    
    // Handle crash overlay
    if (isCrashed) {
        DrawCrashOverlay();
    } else if (crashOverlayBitmap && crashOverlayBitmap->IsVisible()) {
        // Hide crash overlay when crash is over
        crashOverlayBitmap->SetVisible(false);
    }
}

bool Driving::PushEvent(SDL_Event *event) {
    bool consumed = Scene::PushEvent(event);
    lastEvent = event;

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
                SetState(DRIVING_STATE_TURNING_LEFT);
                return true;
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_TURNING_RIGHT);
                return true;
            case SDLK_UP:
                break;
            case SDLK_1:
                if (racing) {
                    LOG_INFO("Switching to Road Race track\n");
                    racing->LoadTrackData(state.GetAssets().GetSrExeData(), SR_TRACK_ROAD_RACE);
                    Car* currentCar = state.GetCurrentCar();
                    if (currentCar) {
                        bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                        racing->SetTransmissionType(isAutomatic);
                    }
                    return true;
                }
                break;
            case SDLK_2:
                if (racing) {
                    LOG_INFO("Switching to Drag Race track\n");
                    racing->LoadTrackData(state.GetAssets().GetSrExeData(), SR_TRACK_DRAG_RACE);
                    Car* currentCar = state.GetCurrentCar();
                    if (currentCar) {
                        bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                        racing->SetTransmissionType(isAutomatic);
                    }
                    return true;
                }
                break;
        }
    } else if (event->type == SDL_KEYUP) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_NORMAL);
                return true;
            case SDLK_UP:
                break;
        }
    }
    return consumed;
}

// Timer callback no longer needed - racing is updated in Draw() method
// Uint32 Driving::AnimationTimerCallback(Uint32 interval, void* param) {
//     Driving* d = static_cast<Driving*>(param);
//     d->racing->Draw(state.GetElapsedTime() / 1000.0f, d->lastEvent);
//     return interval;
// }

void Driving::DrawCrashOverlay() {
    // Show the crash overlay bitmap
    if (crashOverlayBitmap) {
        crashOverlayBitmap->SetVisible(true);
        crashOverlayBitmap->Draw();
    }
}
