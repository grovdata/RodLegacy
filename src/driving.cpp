#include "driving.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include <fstream>
#include <vector>
#include <cstring>

Driving::Driving() : Scene() {
    SDL_Log("Driving::Driving() constructor called\n");
    // Create racing component
    racing = std::make_unique<Racing>();
    roadTexture = nullptr;
    lastEvent = nullptr;
    
    // Initialize track reader
    trackReader = std::make_unique<TrackReader>();
    SDL_Log("Driving constructor completed - trackReader created\n");
}

void Driving::Init(bool debugMode) {
    SDL_Log("Driving::Init() called with debugMode=%s\n", debugMode ? "true" : "false");
    Scene::Init(debugMode);
    
    
    auto dashboard = [&]() {        
        StandardCarColor carColor = state.GetCurrentCar()->GetColor();
        std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
        byte_vector& libFile = state.assets.GetLibPData();
        return state.assets.GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_INTERIOR_1}, carPalette);    
    }();
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);
    AddDrawable("dashboard", std::shared_ptr<IDrawable>(dashboard));

    // Load pillar (windshield beam)
    auto pillar = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WINDSHIELD_BEAM});
    pillar->AlignAbove(dashboard);
    pillar->SetVisible(true);
    AddDrawable("pillar", std::shared_ptr<IDrawable>(pillar));

    // Load roof (ceiling)
    auto roof = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CEILING});
    roof->AlignAbove(pillar);
    roof->SetVisible(true);
    AddDrawable("roof", std::shared_ptr<IDrawable>(roof));

    // Load hood with current car's color
    auto hood = [&]() {        
        StandardCarColor carColor = state.GetCurrentCar()->GetColor();
        std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
        byte_vector& libFile = state.assets.GetLibPData();
        return state.assets.GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD}, carPalette);    
    }();
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    AddDrawable("hood", std::shared_ptr<IDrawable>(hood));

    // Load mirror stem
    // auto mirrorStem = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_THING3});
    // mirrorStem->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    // mirrorStem->SetVisible(true);
    // AddDrawable("mirror_stem", std::shared_ptr<IDrawable>(mirrorStem));

    // Load mirror
    auto mirror = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_REARVIEWMIRROR_FRAME});
    mirror->SetPosition(240, 19);
    mirror->SetVisible(true);
    AddDrawable("mirror", std::shared_ptr<IDrawable>(mirror));
    
    // Load clock for automatic transmission cars (will overlap tachometer)
    auto clock = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_CLOCK_11_50});
    clock->SetPosition(120, 115);
    clock->SetVisible(false); // Will be shown only for automatic transmission
    AddDrawable("clock", std::shared_ptr<IDrawable>(clock));

    // Load plush dice
    auto plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE1});
    plushDice->SetPosition(259, 45);
    plushDice->SetVisible(true);
    AddDrawable("plush_dice1", std::shared_ptr<IDrawable>(plushDice));

    plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE2});
    plushDice->SetPosition(253, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice2", std::shared_ptr<IDrawable>(plushDice));

    plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE3});
    plushDice->SetPosition(250, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice3", std::shared_ptr<IDrawable>(plushDice));

    plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE4});
    plushDice->SetPosition(267, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice4", std::shared_ptr<IDrawable>(plushDice));

    plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE5});
    plushDice->SetPosition(274, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice5", std::shared_ptr<IDrawable>(plushDice));

    // Load wheel left hand 1
    wheelLeftHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_1});
    wheelLeftHand1->SetPosition(72, 144);
    wheelLeftHand1->SetVisible(true);
    AddWheelDrawable("wheel_left_hand_1", wheelLeftHand1);

    wheelLeftHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_2});
    wheelLeftHand2->SetPosition(72, 120);
    wheelLeftHand2->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_2", wheelLeftHand2);

    wheelLeftHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_3});
    wheelLeftHand3->SetPosition(24, 144);
    wheelLeftHand3->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_3", wheelLeftHand3);

    wheelLeftHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_4});
    wheelLeftHand4->SetPosition(72, 144);
    wheelLeftHand4->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_4", wheelLeftHand4);

    // Load wheel right hand 1
    wheelRightHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_1});
    wheelRightHand1->SetPosition(216, 144);
    wheelRightHand1->SetVisible(true);
    AddWheelDrawable("wheel_right_hand_1", wheelRightHand1);

    // Load wheel right hand 2
    wheelRightHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_2});
    wheelRightHand2->SetPosition(216, 120);
    wheelRightHand2->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_2", wheelRightHand2);

    // Load wheel right hand 3
    wheelRightHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_3});
    wheelRightHand3->SetPosition(216, 144);
    wheelRightHand3->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_3", wheelRightHand3);

    // Load wheel right hand 4
    wheelRightHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_4});
    wheelRightHand4->SetPosition(272, 144);
    wheelRightHand4->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_4", wheelRightHand4);

    wheelTurningLeft = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT});
    wheelTurningLeft->SetPosition(120, 144);
    wheelTurningLeft->SetVisible(false);
    AddWheelDrawable("wheel_turning_left", wheelTurningLeft);

    wheelTurningRight = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT});
    wheelTurningRight->SetPosition(120, 144);
    wheelTurningRight->SetVisible(false);
    AddWheelDrawable("wheel_turning_right", wheelTurningRight);

    wheelTurningLeftOpposite = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT_OPPOSITE_1});
    wheelTurningLeftOpposite->SetPosition(72, 144);
    wheelTurningLeftOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_left_opposite", wheelTurningLeftOpposite);

    wheelTurningRightOpposite = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_1});
    wheelTurningRightOpposite->SetPosition(216, 144);
    wheelTurningRightOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_right_opposite", wheelTurningRightOpposite);

    SetState(DRIVING_STATE_NORMAL);
    
    // Load track data for debugging/development
    LoadTrackData();
}

void Driving::LoadTrackData() {
    SDL_Log("Driving::LoadTrackData() called\n");
    
    // Check if trackReader exists
    if (!trackReader) {
        SDL_Log("ERROR: trackReader is null!\n");
        return;
    }
    
    // Check if AssetManager has executable data (where track data is stored)
    byte_vector& srExeData = state.assets.GetSrExeData();
    SDL_Log("AssetManager SrExeData size: %zu bytes\n", srExeData.size());
    
    if (srExeData.empty()) {
        SDL_Log("ERROR: AssetManager SrExeData is empty!\n");
        return;
    }
    
    // Load Street Rod executable data from AssetManager
    if (trackReader->LoadTrackData(srExeData)) {
        SDL_Log("=== Street Rod Track Data Loaded ===\n");
        
        // Load road race track
        SDL_Log("Extracting road race track coordinates...\n");
        auto roadRaceSegments = trackReader->ExtractTrackCoordinates(SR_TRACK_ROAD_RACE, 500);
        SDL_Log("Found %zu road race segments\n", roadRaceSegments.size());
        if (!roadRaceSegments.empty()) {
            auto analysis = trackReader->AnalyzeTrack(roadRaceSegments);
            SDL_Log("Road Race Track:\n");
            SDL_Log("  Segments: %d\n", analysis.segmentCount);
            SDL_Log("  Curved: %d, Straight: %d\n", analysis.curvedSegments, analysis.straightSegments);
            SDL_Log("  Length: %.1f units\n", analysis.totalLength);
            SDL_Log("  Bounds: (%d,%d) to (%d,%d)\n", analysis.minX, analysis.minY, analysis.maxX, analysis.maxY);
            SDL_Log("  Unique sprites: %zu\n", analysis.uniqueSprites.size());
        }
        
        // Load drag race track
        SDL_Log("Extracting drag race track coordinates...\n");
        auto dragRaceSegments = trackReader->ExtractTrackCoordinates(SR_TRACK_DRAG_RACE, 200);
        SDL_Log("Found %zu drag race segments\n", dragRaceSegments.size());
        if (!dragRaceSegments.empty()) {
            auto analysis = trackReader->AnalyzeTrack(dragRaceSegments);
            SDL_Log("Drag Race Track:\n");
            SDL_Log("  Segments: %d\n", analysis.segmentCount);
            SDL_Log("  Curved: %d, Straight: %d\n", analysis.curvedSegments, analysis.straightSegments);
            SDL_Log("  Length: %.1f units\n", analysis.totalLength);
            SDL_Log("  Bounds: (%d,%d) to (%d,%d)\n", analysis.minX, analysis.minY, analysis.maxX, analysis.maxY);
        }
        
        // Load roadside sprites
        SDL_Log("Extracting roadside sprites...\n");
        auto roadsideSprites = trackReader->ExtractRoadSideSprites(SR_ROADSIDE_SPRITES_OFFSET, 100);
        SDL_Log("Found %zu roadside sprite records\n", roadsideSprites.size());
        if (!roadsideSprites.empty()) {
            SDL_Log("Roadside Sprites: %zu records\n", roadsideSprites.size());
            
            // Show first few sprites for debugging
            for (size_t i = 0; i < std::min<size_t>(5, roadsideSprites.size()); ++i) {
                const auto& sprite = roadsideSprites[i];
                SDL_Log("  %s at pos %d, %s at pos %d\n",
                    TrackReader::GetSpriteName(sprite.spriteId1), sprite.trackPosition1,
                    TrackReader::GetSpriteName(sprite.spriteId2), sprite.trackPosition2);
            }
        }
        
        // Load track data into racing component
        if (racing) {
            SDL_Log("Loading track data into racing component...\n");
            racing->LoadTrackData(srExeData, SR_TRACK_ROAD_RACE);
            
            // Configure racing physics based on current car's transmission
            Car* currentCar = state.GetCurrentCar();
            if (currentCar) {
                // Automatic transmissions are types 0-2 (SR_TRANS_AUTO_GM, SR_TRANS_AUTO_FORD, SR_TRANS_AUTO_CHRYSLER)
                bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                racing->SetTransmissionType(isAutomatic);
                racing->SetTransmissionType(false);
                SDL_Log("Car transmission: %s\n", isAutomatic ? "Automatic" : "Manual");
            }
        }
        
        SDL_Log("================================\n");
    } else {
        SDL_Log("Failed to load Street Rod track data from AssetManager executable data.\n");
        SDL_Log("Make sure the Street Rod executable is properly loaded.\n");
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
    AddDrawable(id, std::shared_ptr<IDrawable>(bitmap));
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
}

void Driving::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
    lastEvent = event;
    
    // Handle keyboard input for steering
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
                SetState(DRIVING_STATE_TURNING_LEFT);
                break;
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_TURNING_RIGHT);
                break;
            case SDLK_UP:
                // UP key is handled by racing logic for acceleration
                break;
            case SDLK_1:
                // Switch to Road Race track
                if (racing) {
                    SDL_Log("Switching to Road Race track\n");
                    racing->LoadTrackData(state.assets.GetSrExeData(), SR_TRACK_ROAD_RACE);
                    
                    // Update transmission type
                    Car* currentCar = state.GetCurrentCar();
                    if (currentCar) {
                        bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                        racing->SetTransmissionType(isAutomatic);
                    }
                }
                break;
            case SDLK_2:
                // Switch to Drag Race track
                if (racing) {
                    SDL_Log("Switching to Drag Race track\n");
                    racing->LoadTrackData(state.assets.GetSrExeData(), SR_TRACK_DRAG_RACE);
                    
                    // Update transmission type
                    Car* currentCar = state.GetCurrentCar();
                    if (currentCar) {
                        bool isAutomatic = (currentCar->GetGearbox().assetId <= SR_TRANS_AUTO_CHRYSLER);
                        racing->SetTransmissionType(isAutomatic);
                    }
                }
                break;
        }
    } else if (event->type == SDL_KEYUP) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_NORMAL);
                break;
            case SDLK_UP:
                // UP key is handled by racing logic for acceleration
                break;
        }
    }
}

// Timer callback no longer needed - racing is updated in Draw() method
// Uint32 Driving::AnimationTimerCallback(Uint32 interval, void* param) {
//     Driving* d = static_cast<Driving*>(param);
//     d->racing->Draw(state.GetElapsedTime() / 1000.0f, d->lastEvent);
//     return interval;
// }
