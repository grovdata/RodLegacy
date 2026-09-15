#include "core/log.h"
#include "race/driving.h"
#include "core/state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "race/reputation.h"
#include "core/mouse.h"
#include "core/utils.h"
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>

extern Mouse mouse;  // defined in main.cpp

// The original writes the door mirror straight into the interior bitmap
// (RenderMirrorView 0x238318); RodLegacy lets Racing paint the same pixels into
// the road surface instead, so punch the glass out of the interior and the copy
// shows through. The bitmap bottom-aligns at screen y 100, so bitmap y is screen
// y - 100. kSideMirrorGlass (racing.h) carries the shape both sides share.
static void PunchSideMirrorGlass(Bitmap* interior) {
    SDL_Surface* s = interior ? interior->GetSurface() : nullptr;
    if (!s || s->format->BytesPerPixel != 4) {
        return;
    }
    SDL_LockSurface(s);
    Uint32* px = static_cast<Uint32*>(s->pixels);
    const int stride = s->pitch / 4;
    for (int row = 0; row < kSideMirrorRows; ++row) {
        const SideMirrorRow& g = kSideMirrorGlass[row];
        const int y = kSideMirrorTop - (GAME_HEIGHT - GAME_STATUSBAR_HEIGHT - s->h) + row;
        if (y < 0 || y >= s->h || g.right >= s->w) {
            LOG_ERROR("Driving: door mirror glass row %d falls outside the interior bitmap", row);
            break;
        }
        for (int x = g.left; x <= g.right; ++x) {
            px[y * stride + x] = 0;   // RGBA all zero: fully transparent
        }
    }
    SDL_UnlockSurface(s);
    interior->UpdateFromSurface();
}

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

    // Fresh race: clear any leftover race-end pause state.
    raceEnded = false;
    raceEndPendingKing = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;

    ApplyCarLivery();

    Bitmap* dashboard = dynamic_cast<Bitmap*>(GetDrawable("dashboard"));

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
    auto plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE1_NEUTRAL});
    plushDice->SetPosition(259, 45);
    plushDice->SetVisible(true);
    AddDrawable("plush_dice1", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE2_LEFT1});
    plushDice->SetPosition(253, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice2", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE3_LEFT2});
    plushDice->SetPosition(250, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice3", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE4_RIGHT1});
    plushDice->SetPosition(267, 45);
    plushDice->SetVisible(false);
    AddDrawable("plush_dice4", plushDice);

    plushDice = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE5_RIGHT2});
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

    LoadShifter();

    SetState(DRIVING_STATE_NORMAL);
    
    // Blood on the windscreen, drawn over the wrecked interior on a hard enough
    // impact. Not car-coloured, so it keeps the plain driving palette.
    auto* crashBloodAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CRASHED_BLOOD});
    if (crashBloodAsset) {
        crashBloodBitmap = std::make_unique<Bitmap>(*crashBloodAsset);
        crashBloodBitmap->SetVisible(false);
        crashBloodBitmap->SetPosition(104, 27);
    }
    
    // Load track data for debugging/development
    LoadTrackData();

    if (racing) {
        racing->SetDebugFeaturesEnabled(debugMode);
    }
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
            // Builds the race sim off whatever car is current, gearbox
            // included - nothing to configure separately here.
            racing->LoadTrackData(srExeData, SR_TRACK_ROAD_RACE);

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

// Everything in the cockpit that carries the car's own colour. Init runs once
// at game start, so this has to re-run per race (OnFadeComplete) or every race
// after the first is driven in whatever car was current at startup.
void Driving::ApplyCarLivery() {
    Car* car = state.GetCurrentCar();
    if (!car) return;

    // Hood colour ramp for the assets that carry hood pixels. The Amiga
    // copperlist reloads the hood scanlines' COLOR06/COLOR07 with shades 2 and 3
    // of the car's ramp: the reference frame's hood is 0x0962/0x0A73, and those
    // two words occur exactly once in the Amiga executable, as entries 2 and 3
    // of the golden brown ramp. GetBitmapAssetWithPalette maps palette slot N to
    // carPalette[N-1], so the two shades have to sit at 5 and 6.
    std::vector<uint16_t> hoodPalette = GetStandardCarColor(car->GetColor());
    if (hoodPalette.size() >= 7) {
        hoodPalette[5] = hoodPalette[2];
        hoodPalette[6] = hoodPalette[3];
    }

    byte_vector& libFile = state.GetAssets().GetLibPData();

    // The dashboard bitmap includes the hood's left edge, so it takes the same
    // palette as the hood itself.
    Bitmap* dashboard = state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_INTERIOR_1}, hoodPalette);
    PunchSideMirrorGlass(dashboard);
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);
    SetDrawable("dashboard", dashboard);

    Bitmap* hood = state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD}, hoodPalette);
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    SetDrawable("hood", hood);

    // The wrecked interior shows the buckled hood, so it takes the same ramp as
    // the intact one rather than the raw car palette.
    Bitmap* crashOverlayAsset = state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_CRASHED_INTERIOR}, hoodPalette);
    if (crashOverlayAsset) {
        crashOverlayBitmap = std::make_unique<Bitmap>(*crashOverlayAsset);
        crashOverlayBitmap->SetVisible(false);
        crashOverlayBitmap->SetPosition(0, 18);
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

    // The right hand is on the shifter for the length of a shift, so it cannot
    // also be on the wheel. Here rather than in UpdateShifter because the
    // steering can re-run SetState mid-shift.
    if (shiftPhase != ShiftPhase::Idle) {
        wheelRightHand1->SetVisible(false);
        wheelRightHand2->SetVisible(false);
        wheelRightHand3->SetVisible(false);
        wheelRightHand4->SetVisible(false);
    }
}

void Driving::OnSceneEnter() {
    // No music while racing. Init() runs once at game start for every scene,
    // so pausing there would kill music for the whole session.
    if (AudioManager* am = state.GetAssets().GetAudioManager()) {
        musicWasPaused = am->IsMusicPaused();
        am->PauseMusic();
    }

    if (!racing) return;

    // Before the track loads (which builds the race sim off the current car):
    // this race may be in a different car than the last one, or than the one
    // that was current when Init ran.
    ApplyCarLivery();

    RaceType pendingType = state.GetPendingRaceType();
    SrTrackType trackType = (pendingType == RaceType::Drag)
                            ? SR_TRACK_DRAG_RACE : SR_TRACK_ROAD_RACE;

    // Load the appropriate track (falls back to existing data if EXE not available)
    byte_vector& srExeData = state.GetAssets().GetSrExeData();
    if (!srExeData.empty()) {
        racing->LoadTrackData(srExeData, trackType);
    } else {
        racing->SetTrackType(trackType);
    }

    // Scene-side per-race state. Init() runs once at game start, so the
    // race-end pause and its bubble are cleared here instead.
    raceEnded = false;
    raceEndPendingKing = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;
    wreckDialog.reset();
    raceEndDismissAllowedMs = 0;
    mouse.SetVisible(false);

    // Dice pendulum starts from rest.
    currentDiceIndex = 0;
    diceTilt = 0;
    diceDir = 1;
    diceStepTimer = 0.0f;

    racing->ResetForNewRace();
    racing->InitializeOpponent();

    // After ResetForNewRace: the gate plate to show depends on this car's box.
    ResetShifter();

    LOG_INFO("Driving::OnSceneEnter - race starting: type=%s bet=%d pinkSlip=%d",
             pendingType == RaceType::Drag ? "Drag" : "Road",
             state.GetPendingBetAmount(),
             state.GetPendingIsPinkSlip() ? 1 : 0);
}

// The gear shifter. RodLegacyResearch/docs/driving/shifter.md has the full
// write-up; the short version is that the knob walks a gate path one stop per
// race frame with a hand on it, and sits bare at each end.
//
// Gate paths, offsets from the plate's top-left. The manual one is an
// H-pattern whose odd entries are the route through the gate, so 2 -> 3 walks
// back to centre-left, across, then up-right.
static constexpr SDL_Point kGateManual[9] = {
    {22, 19}, {22, 11}, {22, 19}, {22, 29}, {22, 19},
    {31, 19}, {31, 11}, {31, 19}, {31, 29}
};
static constexpr SDL_Point kGateAuto[3] = {{28, 21}, {28, 27}, {28, 34}};
// Where each gear rests on the manual path; an automatic uses 0 (N) and 2 (D).
static constexpr int kGearStop[5] = {0, 1, 3, 6, 8};

void Driving::LoadShifter() {
    const AmigaLibAsset plates[3] = {RACING_GEARBOX_AUTO, RACING_GEARBOX_MANUAL_3SPEED,
                                     RACING_GEARBOX_MANUAL_4SPEED};
    const AmigaLibAsset knobs[3] = {RACING_GEARBOX_KNOB_AUTOMATIC, RACING_GEARBOX_KNOB_3SPEED,
                                    RACING_GEARBOX_KNOB_4SPEED};
    for (int i = 0; i < 3; ++i) {
        shifterPlate[i] = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, plates[i]});
        shifterPlate[i]->SetVisible(false);
        AddDrawable("shifter_plate_" + std::to_string(i), shifterPlate[i]);
        shifterKnob[i] = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, knobs[i]});
        shifterKnob[i]->SetVisible(false);
        AddDrawable("shifter_knob_" + std::to_string(i), shifterKnob[i]);
    }
    shifterHand = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RIGHT_HAND_SHIFTING});
    shifterHand->SetVisible(false);
    AddDrawable("shifter_hand", shifterHand);

    // The plate sits at (0x110, 0x2c) in the dashboard bitmap, which we draw
    // bottom-aligned, so anchor to it rather than to the screen. The dashboard
    // is positioned once, just above, and never moves.
    auto* dash = dynamic_cast<Bitmap*>(GetDrawable("dashboard"));
    SDL_Rect dashRect = dash ? dash->GetPosition() : SDL_Rect{0, 0, 0, 0};
    shifterOrigin = {dashRect.x + 0x110, dashRect.y + 0x2c};
    for (Bitmap* plate : shifterPlate) plate->SetPosition(shifterOrigin.x, shifterOrigin.y);
}

void Driving::ResetShifter() {
    shiftPhase = ShiftPhase::Idle;
    shiftStepTimer = 0.0f;
    shifterKind = 2;
    if (racing) {
        shifterKind = racing->IsAutomatic() ? 0 : (racing->GetNumGears() == 3 ? 1 : 2);
    }
    shiftStop = 0;
    shiftCursor = 0;

    for (int i = 0; i < 3; ++i) {
        shifterPlate[i]->SetVisible(false);
        shifterKnob[i]->SetVisible(false);
    }
    shifterHand->SetVisible(false);
}

void Driving::UpdateShifter(float deltaTime) {
    if (!racing || !shifterHand) return;

    const bool automatic = (shifterKind == 0);
    const SDL_Point* gate = automatic ? kGateAuto : kGateManual;
    // An automatic's lever shows the selector, not the gear.
    const int gear = std::clamp<int>(racing->GetCurrentGear(), 0, 4);
    const int wanted = automatic ? (racing->IsAutoDriveEngaged() ? 2 : 0) : kGearStop[gear];

    // The wrecked interior replaces the dashboard, and the flyby has no cockpit.
    const bool hidden = racing->IsFlybyMode() || racing->ShouldShowCrashOverlay();
    if (hidden) {
        shiftPhase = ShiftPhase::Idle;
        shiftStop = wanted;
    } else if (shiftPhase == ShiftPhase::Idle && wanted != shiftStop) {
        shiftPhase = ShiftPhase::KnobStart;
        shiftCursor = shiftStop;
        shiftTarget = wanted;
        shiftStep = (shiftStop < wanted) ? 1 : -1;
        shiftStepTimer = 0.0f;
        SetState(currentState);   // takes the right hand off the wheel
    }

    if (shiftPhase != ShiftPhase::Idle) {
        shiftStepTimer += deltaTime * 1000.0f;
        while (shiftPhase != ShiftPhase::Idle && shiftStepTimer >= SHIFT_STEP_MS) {
            shiftStepTimer -= SHIFT_STEP_MS;
            switch (shiftPhase) {
                case ShiftPhase::KnobStart:
                    shiftPhase = ShiftPhase::Slide;
                    break;
                case ShiftPhase::Slide:
                    if (shiftCursor == shiftTarget) shiftPhase = ShiftPhase::KnobEnd;
                    else shiftCursor += shiftStep;
                    break;
                case ShiftPhase::KnobEnd:
                    shiftStop = shiftTarget;
                    shiftPhase = ShiftPhase::Idle;
                    SetState(currentState);   // hand back on the wheel
                    break;
                case ShiftPhase::Idle:
                    break;
            }
        }
    }

    // Nothing of the gearbox shows at rest; during a shift the plate is up,
    // with the bare knob at each end and the hand on it for the slide.
    const bool active = (shiftPhase != ShiftPhase::Idle);
    shifterPlate[shifterKind]->SetVisible(active);
    shifterKnob[shifterKind]->SetVisible(active && shiftPhase != ShiftPhase::Slide);
    shifterHand->SetVisible(shiftPhase == ShiftPhase::Slide);
    if (!active) return;

    if (shiftPhase == ShiftPhase::Slide) {
        // Top-left anchored at (0xfc, 0x22) - up and left of the plate, since
        // the arm reaches in from that side.
        const SDL_Point at = gate[shiftCursor];
        shifterHand->SetPosition(shifterOrigin.x + at.x - 20, shifterOrigin.y + at.y - 10);
    } else {
        const SDL_Point at = gate[shiftPhase == ShiftPhase::KnobStart ? shiftStop : shiftTarget];
        SDL_Rect knob = shifterKnob[shifterKind]->GetNaturalSize();
        shifterKnob[shifterKind]->SetPosition(shifterOrigin.x + at.x - knob.w / 2,
                                              shifterOrigin.y + at.y + 5 - knob.h / 2);
    }
}

void Driving::UpdateDiceAnimation(float deltaTime) {
    // The plush dice swing as a continuous pendulum, matching the original
    // (RenderPlayerCar 0x238b28): a tilt -2..2 steps +-1 each race frame and
    // reverses at its bounds, so the dice keep moving even on a straight road.
    // Steering shifts one bound so the swing biases away from the turn (inertia).
    // When the car is stopped it swings on until it crosses centre, then settles.
    const bool moving = racing && racing->GetSpeed() > 0.1f;
    const int steer = racing ? racing->GetSteerDisplay() : 0;   // <0 left, >0 right
    int lo = -2;
    int hi = 2;
    if (steer < 0) {
        lo = -1;    // steering left: bias the swing toward the right (+)
    } else if (steer > 0) {
        hi = 1;     // steering right: bias the swing toward the left (-)
    }

    diceStepTimer += deltaTime * 1000.0f;
    while (diceStepTimer >= DICE_SWING_STEP_MS) {
        diceStepTimer -= DICE_SWING_STEP_MS;
        if (!moving && diceTilt == 0) {
            diceStepTimer = 0.0f;   // stopped and centred: hold
            break;
        }
        if (diceTilt >= hi) {
            diceDir = -1;
        } else if (diceTilt <= lo) {
            diceDir = 1;
        }
        diceTilt += diceDir;
    }

    // Map tilt -2..2 to the frame in screen-x order: -2 LEFT2(2), -1 LEFT1(1),
    // 0 NEUTRAL(0), +1 RIGHT1(3), +2 RIGHT2(4).
    const int mag = std::abs(diceTilt);
    const int idx = (diceTilt < 0) ? mag : (diceTilt > 0 ? 2 + mag : 0);
    if (idx == currentDiceIndex) {
        return;
    }
    auto oldDice = GetDrawable("plush_dice" + std::to_string(currentDiceIndex + 1));
    if (oldDice) {
        oldDice->SetVisible(false);
    }
    currentDiceIndex = idx;
    auto newDice = GetDrawable("plush_dice" + std::to_string(idx + 1));
    if (newDice) {
        newDice->SetVisible(true);
    }
}

void Driving::DrawScene() {
    Scene::DrawScene();

    // Update racing simulation
    if (racing) {
        // While the race-end bubble is up the scene is frozen (no ticking); the
        // last rendered frame stays on screen until any key returns to the garage.
        if (!raceEnded) {
            // Get frame delta time in seconds, with a reasonable maximum to prevent huge jumps
            float deltaTime = state.GetElapsedTime() / 1000.0f;
            if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap at 100ms to prevent huge jumps

            racing->Draw(deltaTime, lastEvent);

            if (cruising) {
                if (racing->CruiseFinished()) {
                    EndCruise();
                    return;
                }
            } else if (!racing->IsFlybyMode()) {
                // Race ended: record the result, then pause and show the reason.
                RaceOutcome outcome = racing->GetRaceOutcome();
                if (outcome != RaceOutcome::Ongoing) {
                    bool won = (outcome == RaceOutcome::PlayerWon);
                    RaceType raceType = (racing->GetTrackType() == SR_TRACK_DRAG_RACE)
                                        ? RaceType::Drag : RaceType::Road;
                    const srrace::RaceEndContext endCtx = racing->BuildRaceEndContext();
                    std::string reason = srrace::BuildRaceEndReason(endCtx);
                    const srrace::CrashOutcome crash = srrace::ClassifyCrash(endCtx);
                    // Speeding ticket. Read here because ResetRaceOutcome below
                    // clears the sim's cop state; it is charged further down,
                    // after the tires, because what the player still owes for
                    // those is added to the fine.
                    const bool busted = racing->PlayerBusted();
                    const int speedingFine = busted ? racing->SpeedingFine() : 0;

                    // The rivalry with this driver, tallied whatever the
                    // outcome - a ticket voids the race for everything else
                    // but still counts as a meeting.
                    state.RecordChallengerRace(!won && !busted);
                    // Read before OnRaceFinished, which does not clear it, but
                    // checking first is one less thing to get wrong later.
                    const bool wasKingRace = (state.GetLastChallengerPoolIndex() == RACER_KING_SLOT);
                    // A ticket is the whole outcome (srrace::SettlesBet): the
                    // stake, the reputation and the racer pool are left alone,
                    // so being pulled over in a pink-slip race no longer takes
                    // the car with it.
                    raceEndPendingKing = srrace::SettlesBet(endCtx)
                                             ? state.OnRaceFinished(raceType, won)
                                             : false;
                    racing->ResetRaceOutcome();
                    // Running the race itself costs fuel, on top of the drive
                    // legs State::MoveToScene charges.
                    state.BurnFuel(raceType == RaceType::Drag
                                       ? State::FUEL_PER_DRAG_RACE_TENTHS
                                       : State::FUEL_PER_ROAD_RACE_TENTHS);
                    // King races advance the clock 600/1050 ticks (@150/hr =
                    // 4h/7h) instead of the usual flat 2h - see king.md.
                    if (wasKingRace) {
                        state.AdvanceGameHours(raceType == RaceType::Drag ? 4 : 7);
                        // Confirmed from the disassembly's string table
                        // (LAB_1551 / LAB_1552+LAB_1553). No King-specific
                        // loss line has turned up, so a loss still uses the
                        // generic crash/lose reason below.
                        if (won) {
                            reason = raceType == RaceType::Drag
                                ? "Beginner's luck, punk!"
                                : "You ran a good race. You deserve to be King of the County!";
                        }
                    } else {
                        state.AdvanceGameHours(2);
                    }

                    // Wear first, damage second: a race that blew the engine
                    // strips it straight after, so the order only matters for
                    // the races that end intact - which is all of them.
                    ApplyRaceWear();
                    // What a fresh set of tires left unpaid, if the player could
                    // not cover the $50. The original returns it from
                    // ReplaceBlownTires (0x245044, named ApplyCrashDamage until
                    // the evidence said otherwise) and both its callers spend it.
                    int tireDebt = 0;
                    if (endCtx.tiresBlown) {
                        tireDebt = ApplyBlownTires();
                    }
                    ApplyDrivetrainDamage(endCtx.endFlags);

                    // Charged here rather than where it is read: ProcessRaceOutcome's
                    // cop-caught block runs `sVar8 = ApplyCrashDamage()` and then
                    // adds the speed-tiered fine to it, so the tires come out of
                    // the bankroll first and whatever they could not cover is
                    // billed on top of the ticket.
                    if (busted) {
                        const int owed = speedingFine + tireDebt;
                        if (state.GetBankroll() >= owed) {
                            state.SetBankroll(state.GetBankroll() - owed);
                        } else {
                            // Short of it: hauled off to the slammer and the run
                            // is over (outcome 4). The diner draws itself behind
                            // bars from here on - docs/diner.md ("Jail").
                            state.SetJailed(true);
                        }
                    }

                    // Anything ProcessRaceOutcome answers 2 to sends the player
                    // home: a wreck that cost money (repaired or totaled), a
                    // drivetrain that let go, and still owing for the tires -
                    // its ticket branch reads `(sVar8 = ApplyCrashDamage(),
                    // sVar8 == 0)` and falls to 2 when it is not. See
                    // LeaveAfterRace.
                    const bool drivetrainFailed =
                        (endCtx.endFlags &
                         (srrace::END_ENGINE_BLOWN | srrace::END_TRANS_DROPPED)) != 0;
                    raceEndLeftTheDiner = (crash == srrace::CrashOutcome::Totaled ||
                                           crash == srrace::CrashOutcome::RepairOrJunk ||
                                           crash == srrace::CrashOutcome::Forfeited ||
                                           drivetrainFailed ||
                                           tireDebt > 0);

                    // What the wreck costs, after the bet has been settled -
                    // the order ProcessRaceOutcome uses.
                    if (crash == srrace::CrashOutcome::Totaled) {
                        const int scrap = srrace::ScrapPayout(endCtx.carStrippedValue);
                        state.SetBankroll(state.GetBankroll() + scrap);
                        state.ScrapCurrentCar();
                        LOG_INFO("Driving: car totaled at %d mph, $%d for scrap",
                                 endCtx.crashSpeed, scrap);
                    }

                    LOG_INFO("Driving: race ended (%s); press any key for garage",
                             reason.c_str());
                    raceEnded = true;
                    // Dip out and back in first; ShowRaceEndMessage puts the
                    // message up once that is done.
                    raceEndReason = reason;
                    raceEndWon = won;
                    raceEndWreck = (crash == srrace::CrashOutcome::RepairOrJunk);
                    raceEndCtx = endCtx;
                    // The stats panel is only built for a race decided at the
                    // line: ProcessRaceOutcome calls BuildResultText on the win
                    // branch and on a loss only when the outcome word is exactly
                    // 1 (finished, nothing broken, no cop).
                    raceEndShowStats = (endCtx.endFlags & srrace::END_FAILURE_MASK) == 0;
                    raceEndTimeTenths = racing->RaceTimeTenths();
                    raceEndAvgSpeed = racing->AverageSpeedMph();
                    raceEndPercent = racing->DistancePercent();
                    raceEndDipping = true;
                    FadeOut();
                }
            }

            // Dice pendulum swing.
            UpdateDiceAnimation(deltaTime);

            // Gear shifter.
            UpdateShifter(deltaTime);
        }

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
                // Force nearest-neighbor: the global scale-quality hint is set
                // after the renderer is created, so it isn't reliably applied.
                SDL_SetTextureScaleMode(roadTexture, SDL_ScaleModeNearest);
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
    
    // Steering wheel follows the eased steering position (slide velocity), so the
    // dot and hands re-center over several frames after the stick is released
    // instead of snapping. Onset is immediate; release decays to center.
    if (racing && !racing->IsFlybyMode() && !racing->ShouldShowCrashOverlay()) {
        const int steer = racing->GetSteerDisplay();
        DrivingState wheelState = (steer > 0) ? DRIVING_STATE_TURNING_RIGHT
                                : (steer < 0) ? DRIVING_STATE_TURNING_LEFT
                                              : DRIVING_STATE_NORMAL;
        if (wheelState != currentState) {
            SetState(wheelState);
        }
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

    // Red steering-wheel dot (rides the wheel rim, re-centers with the steering).
    DrawSteeringDot();

    // Handle crash state - set proper driving state
    bool isCrashed = racing && !racing->IsFlybyMode() && racing->ShouldShowCrashOverlay();
    
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
        if (crashBloodBitmap) {
            crashBloodBitmap->SetVisible(false);
        }
    }

    if (debugMode && debugOverlay) {
        if (racing && racing->IsFlybyMode()) {
            debugLines.clear();
            const std::vector<std::string> lines =
                racing->GetFlybyDebugLines(racing->GetLastVisibleRow());
            for (const std::string& line : lines) {
                AddDebugLine(line);
            }
        }
        DrawLegacyDebugOverlay();
    }

    if (statsDialog && statsDialog->IsVisible()) {
        statsDialog->Draw();
    }
    // Race-end reason bubble, drawn on top of the frozen scene.
    if (raceEnded && raceEndBubble) {
        raceEndBubble->Draw();
    }
    if (wreckDialog && wreckDialog->IsVisible()) {
        wreckDialog->Draw();
    }
}

// The race-end dip: the Amiga fades the race view out when the frame loop
// exits and fades the same view back in at the top of ProcessRaceOutcome, and
// only then blits the message. See RodLegacyResearch/docs/palette_fade.md.
void Driving::OnFadeComplete() {
    Scene::OnFadeComplete();
    if (!raceEndDipping) {
        return;
    }
    if (sceneTransition == SCENE_BLACK) {
        FadeIn();
        return;
    }
    raceEndDipping = false;
    ShowRaceEndMessage();
}

// Dialog 24: two lines of race stats across the top of the screen, no buttons.
// The wording is verbatim from the string table (0x24b446..0x24b4b0); the
// original's backticks are line-break markers it renders as spaces here.
void Driving::ShowStatsDialog() {
    char line1[80];
    char line2[80];
    if (raceEndWon) {
        snprintf(line1, sizeof(line1), "Your winning time %d.%d sec., avg. speed %2d mph",
                 raceEndTimeTenths / 10, raceEndTimeTenths % 10, raceEndAvgSpeed);
        snprintf(line2, sizeof(line2), "Congratulations !");
    } else {
        snprintf(line1, sizeof(line1), "In %d.%d sec you covered only %2d%%",
                 raceEndTimeTenths / 10, raceEndTimeTenths % 10, raceEndPercent);
        snprintf(line2, sizeof(line2), "of the distance. Your avg. speed %2d mph.",
                 raceEndAvgSpeed);
    }

    const SDL_Rect bounds = {7, 7, 255, 28};
    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0, 208, 0, 255};
    cfg.borderColor = {0, 111, 239, 255};
    cfg.contentAreas = std::vector<SDL_Rect>{bounds};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = {
        DialogTextConfig{SDL_Rect{10, 11, 249, 10}, line1, {0xFF, 0xFF, 0xFF, 0xFF}, true},
        DialogTextConfig{SDL_Rect{10, 21, 249, 10}, line2, {0xFF, 0xFF, 0xFF, 0xFF}, true},
    };

    statsDialog = std::make_unique<Dialog>(cfg);
    statsDialog->Show();
}

void Driving::ShowRaceEndMessage() {
    if (raceEndShowStats) {
        ShowStatsDialog();
    }
    if (raceEndWreck) {
        ShowWreckDialog(raceEndCtx);
    } else {
        raceEndBubble = std::make_unique<Bubble>(raceEndReason, BUBBLE_LEFT, 85, 40, false);
        raceEndBubble->SetVisible(true);
    }
    // Hold every race-end message on screen for a minimum time before any key
    // can dismiss it, so a still-held accelerator (or other key) can't skip the
    // win/loss/crash message.
    raceEndDismissAllowedMs = SDL_GetTicks() + 4000;
    // The race-end screen brings the pointer back, thumb up or down depending
    // on how it went.
    mouse.SetPointerType(raceEndWon ? SrLibAmigaSpriteAssetMousePointerThumbsUp
                                    : SrLibAmigaSpriteAssetMousePointerThumbsDown);
    mouse.SetVisible(true);
}

// The repair quote after a wreck at 31..90 mph (ProcessRaceOutcome dialogs 25
// and 26). Above 60 mph the damage reads as terminal and the quote is worded
// as a parts payout; below it, as getting the car running again. "Fix it" is
// simply absent when the quote is more than the player has, which is what the
// original's disabled-button flag amounts to.
void Driving::ShowWreckDialog(const srrace::RaceEndContext& ctx) {
    std::uniform_int_distribution<int> roll(0, srrace::RepairRollRange(ctx.crashSpeed) - 1);
    const int cost = 100 + roll(RandomEngine());
    const int junk = srrace::JunkPayout(ctx.carValue);
    const bool affordable = cost <= state.GetBankroll();

    const std::string quote = (ctx.crashSpeed > 60)
        ? "$" + std::to_string(cost) + " for parts and you are back on the streets."
        : "Damn. It'll cost $" + std::to_string(cost) + " to get this baby running.";

    SDL_Rect dialogRect = {34, 90, 252, 56};
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{38, 94, 248, 10}, "Oh, well, it could have been worse.",
                         {0x00, 0x00, 0x00, 0xFF}, true},
        DialogTextConfig{SDL_Rect{38, 104, 248, 10}, quote, {0x00, 0x00, 0x00, 0xFF}, true}
    };
    if (ctx.tiresBlown) {
        texts.push_back(DialogTextConfig{SDL_Rect{38, 114, 248, 10}, "Plus tires !",
                                         {0x00, 0x00, 0x00, 0xFF}, true});
    }

    std::vector<ButtonConfig> buttons;
    if (affordable) {
        buttons.push_back({SDL_Rect{88, 126, 66, 16}, "Fix it", nullptr, true, 0});
    }
    buttons.push_back({SDL_Rect{166, 126, 66, 16}, "Junk it", nullptr, !affordable, 1});

    DialogConfig cfg;
    cfg.bounds = dialogRect;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{dialogRect};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = texts;
    cfg.buttons = buttons;
    cfg.onButtonPressed = [this, cost, junk](int index) {
        if (index == 0) {
            state.SetBankroll(state.GetBankroll() - cost);
            LOG_INFO("Driving: wreck repaired for $%d", cost);
        } else {
            state.SetBankroll(state.GetBankroll() + junk);
            state.ScrapCurrentCar();
            LOG_INFO("Driving: wreck junked for $%d", junk);
        }
        SetActiveDialog(nullptr);
        LeaveAfterRace();
    };

    wreckDialog = std::make_unique<Dialog>(cfg);
    SetActiveDialog(wreckDialog.get());
}

// A blown engine takes the whole assembly with it. The original's
// StripAllDrivetrain (0x2453ca) nulls all five part pointers on the car struct
// (engine, manifold, carb 0-2), takes their value back off the car and zeroes
// the six engine bay screws at car+0x24. A dropped transmission is the same
// routine one branch over (0x24540a), clearing the single pointer at car+0x06.
// Both leave the car unable to move until it is rebuilt from bought parts -
// Car::IsRunning goes false, and the garage refuses to drive it anywhere.
// The sim counts wear on the player's engine and gearbox all race long
// (PlayerCarPhysics: +1 each per tick above 5000 rpm, +1 on the engine every
// 16 ticks, +3/+15 on the box per shift) but it works on a copy of the setup,
// so without this the car left every race as fresh as it arrived and no part
// ever wore out.
void Driving::ApplyRaceWear() {
    Car* car = state.GetCurrentCar();
    if (!car || !racing) return;
    car->AddEngineWearRaw(racing->PlayerEngineWearDelta());
    car->AddGearboxWearRaw(racing->PlayerTransWearDelta());

    // Tires are the odd one out: one lump at the end of the race, and only if
    // the car actually went anywhere (InitRaceState's `200 < playerSeg` gate,
    // and the line starts at segment 200).
    const int32_t endSeg = racing->PlayerSegment();
    const bool isRoad = (racing->GetTrackType() != SR_TRACK_DRAG_RACE);
    int tireRaw = 0;
    if (endSeg > srrace::TIRE_WEAR_MIN_SEGMENTS) {
        std::uniform_int_distribution<int> roll300(0, 299);
        tireRaw = srrace::TireWearForRace(static_cast<int>(car->GetWheelType()), isRoad,
                                          roll300(RandomEngine()));
        car->AddTireWearRaw(tireRaw);
    }

    LOG_INFO("Driving: race wear [%s seg=%d car=%p tireClass=%d] engine +%d raw (now %d%%), "
             "trans +%d raw (now %d%%), tires +%d raw (now %d%%)",
             isRoad ? "road" : "drag", endSeg, (void*)car,
             static_cast<int>(car->GetWheelType()),
             racing->PlayerEngineWearDelta(), car->GetEngine().wear,
             racing->PlayerTransWearDelta(), car->GetGearbox().wear,
             tireRaw, car->GetTires().wear);
}

// A blowout is not damage you pay to repair - the original just fits a new set
// on the spot (ApplyCrashDamage 0x245044 / DOS 1000:ba3b): the blown tires come
// off the car and off its value, a spare goes on if there is one, and if there
// is not you are billed $50 for a fresh set (bankroll floored at zero).
// Returns what the player still owes for the new set - 0 unless the $50 ran
// the bankroll below zero. ReplaceBlownTires (0x245044) returns the same number
// and its callers add it to the speeding fine or send the player home with it.
int Driving::ApplyBlownTires() {
    Car* car = state.GetCurrentCar();
    if (!car) return 0;

    const auto& spares = state.GetSpareTires();
    if (!spares.empty()) {
        const Part fresh = spares.front();
        state.RemoveSpareAt(PartCategory::Tire, 0);
        car->SetTires(fresh);
        LOG_INFO("Driving: tires blown - fitted a spare (%d%% worn)", WearPercent(fresh.wear));
        return 0;
    }

    constexpr int kNewTireCharge = 50;
    const int owed = std::max(0, kNewTireCharge - state.GetBankroll());
    state.SetBankroll(std::max(0, state.GetBankroll() - kNewTireCharge));
    car->SetTires(car->GetWheelType());   // a brand-new set of the same kind
    LOG_INFO("Driving: tires blown - no spare, charged $%d, bankroll now $%d, still owed $%d",
             kNewTireCharge, state.GetBankroll(), owed);
    return owed;
}

void Driving::ApplyDrivetrainDamage(uint16_t endFlags) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (endFlags & srrace::END_ENGINE_BLOWN) {
        car->StripEngineAssembly();
        LOG_INFO("Driving: engine blown - engine, manifold and carburetors stripped");
    } else if (endFlags & srrace::END_TRANS_DROPPED) {
        car->RemoveGearbox();
        LOG_INFO("Driving: transmission dropped - gearbox gone");
    }
}

// Called from the wreck dialog's own button callback, so the dialog is hidden
// rather than destroyed here - OnSceneEnter clears it for the next race.
// Cruising in town. The original runs the whole drive through RunRoadRaceWithMode
// with the race flag cleared; here it is the same Racing, started back in the
// approach section - docs/driving/cruising.md.
void Driving::StartCruise(int startSeg, SrLibGameScene destination) {
    cruising = true;
    cruiseDestination = destination;
    raceEnded = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;
    mouse.SetVisible(false);
    if (racing) {
        // Each destination's sign stands before its own building.
        int sign = RACING_SIGN_GARAGE;
        if (destination == SCENE_DINER) {
            sign = RACING_DRIVE_IN;
        } else if (destination == SCENE_GAS_STATION) {
            sign = RACING_SIGN_GUS_GAS;
        }
        racing->StartCruise(startSeg, sign);
    }
}

void Driving::EndCruise() {
    cruising = false;
    state.MoveToScene(cruiseDestination);
}

void Driving::LeaveAfterRace() {
    raceEnded = false;
    raceEndBubble.reset();
    statsDialog.reset();
    if (wreckDialog) wreckDialog->Hide();
    const bool endGame = raceEndPendingKing;
    raceEndPendingKing = false;
    if (endGame) {
        state.MoveToScene(SCENE_KING_OF_COUNTY);
    } else if (state.IsJailed()) {
        // Not the garage: you are in a cell, watching the diner.
        state.MoveToScene(SCENE_DINER);
    } else if (state.GetCurrentCar() == nullptr || raceEndLeftTheDiner) {
        // The garage, and the out-of-dough check that lives in its loop
        // (Garage::OnSceneEnter). With money left, the paper is a click away.
        state.MoveToScene(SCENE_GARAGE);
    } else {
        // Back to the diner for the next challenger. CONFIRMED against
        // ChallengeRaceLoop (0x23e60c): it switches on ProcessRaceOutcome's
        // (0x23eaa2) return, and 1, 2 and 4 all leave the diner loop - 1 and 2
        // via `RunRoadRaceWithMode()` (the drive-away cutscene) and a plain
        // `return`, 4 via ShowOpponentRefusal. Only 3 falls through to the top
        // of the loop, so the next racer walks in after an ordinary win or
        // loss. A blown engine or a dropped transmission is NOT one of those:
        // ProcessRaceOutcome's `else if (bVar5)` branch - taken when the
        // outcome word A4+0x4bc6 is 0x05, 0x11 or 0x15 - strips the drivetrain
        // and returns 2, so the player goes home. That is what
        // raceEndLeftTheDiner covers above.
        state.MoveToScene(SCENE_DINER);
    }
}

bool Driving::PushEvent(SDL_Event *event) {
    // Once the race has ended, any key dismisses the reason bubble and returns
    // to the garage (or the end-game screen if the King was just beaten). The
    // wreck dialog is the exception: it wants a button click, not any key.
    // "You can skip the cruising by pressing the selector" (manual).
    if (cruising && (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN)) {
        EndCruise();
        return true;
    }

    if (raceEnded && !raceEndDipping && !wreckDialog && event->type == SDL_KEYDOWN) {
        // Hold the crash screen for its minimum display time; ignore keys until then.
        if (SDL_GetTicks() < raceEndDismissAllowedMs) {
            return true;
        }
        LeaveAfterRace();
        return true;
    }

    if (debugMode && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_p && racing) {
        std::string path;
        if (racing->SaveRoadViewPng(&path)) {
            LOG_INFO("Saved road view: %s\n", path.c_str());
        }
        return true;
    }

    if (racing && racing->IsFlybyMode() && event->type == SDL_KEYDOWN) {
        if (racing->OnFlybyKeyDown(event->key.keysym.sym)) {
            return true;
        }
    }

    bool consumed = Scene::PushEvent(event);
    lastEvent = event;

    if (debugMode && event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F9) {
        if (racing && racing->ToggleFlybyMode()) {
            return true;
        }
    }

    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_RIGHT:
                // Wheel animation is driven per-frame from the eased steering
                // position (DrawScene), so it re-centers gradually on release.
                return true;
            case SDLK_UP:
                break;
            case SDLK_1:
                if (racing) {
                    LOG_INFO("Switching to Road Race track\n");
                    racing->LoadTrackData(state.GetAssets().GetSrExeData(), SR_TRACK_ROAD_RACE);
                    return true;
                }
                break;
            case SDLK_2:
                if (racing) {
                    LOG_INFO("Switching to Drag Race track\n");
                    racing->LoadTrackData(state.GetAssets().GetSrExeData(), SR_TRACK_DRAG_RACE);
                    return true;
                }
                break;
        }
    } else if (event->type == SDL_KEYUP) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_RIGHT:
                // Do not snap the wheel to center here; the per-frame update lets
                // it ease back over several frames (original slide-velocity decay).
                return true;
            case SDLK_UP:
                break;
        }
    }
    return consumed;
}

void Driving::UpdateDebugInfo() {
    if (!debugMode) {
        return;
    }
    Scene::UpdateDebugInfo();

    // In flyby the camera readout is the only thing worth the limited space of
    // the in-game overlay, so it replaces the generic lines.
    if (racing && racing->IsFlybyMode()) {
        debugLines.clear();
        debugRows.clear();
        for (const std::string& line :
             racing->GetFlybyDebugLines(racing->GetLastVisibleRow())) {
            AddDebugLine(line);
        }
    }
}

std::vector<DebugPanel> Driving::GetDebugPanels() {
    std::vector<DebugPanel> panels = Scene::GetDebugPanels();
    if (racing) {
        // The debug UI has room for both, so the camera readout is its own
        // panel rather than displacing the scene lines.
        // Table of values, plus the flyby key bindings as loose lines when the
        // camera is engaged.
        DebugPanel racingPanel{"Racing", {}};
        racingPanel.toggles.push_back(
            {"Invulnerable (no crash, no over-rev)", racing->DebugInvulnerableFlag()});
        racingPanel.rows = racing->GetRacingDebugRows(racing->GetLastVisibleRow());
        if (racing->IsFlybyMode()) {
            racingPanel.lines = {
                "[ ] step   shift+[ ] seg   ctrl+[ ] +/-10seg",
                "=/- heading   ,/. lateral   Home/End start/finish",
                "PgUp/Dn +/-50seg   N toggle NPC   F9 exit flyby",
                "P save road PNG (no cockpit)",
            };
        } else {
            racingPanel.lines = {"F9 enter flyby"};
        }
        panels.push_back(std::move(racingPanel));

        // Who we are actually racing: identity and driver stats from the roster
        // and SR_data, plus what their AI is doing right now.
        DebugPanel opponent{"Opponent", {}};
        const Opponent* opp = state.GetCurrentOpponent();
        if (opp != nullptr) {
            opponent.rows.push_back({"driver", ""});
            opponent.rows.push_back({"name", opp->driver.name});

            Bitmap* portrait = state.GetAssets().GetBitmapAsset(
                {LibAssetRegular, SrLibFileAmigaLibP,
                 static_cast<AmigaLibAsset>(opp->driver.spriteIndex)});
            if (portrait != nullptr) {
                opponent.image = portrait->GetTexture();
            }

            const SrDriverData* driverData = state.GetCurrentDriverData();
            if (driverData != nullptr) {
                // Both stats fall as skill rises - lower is better in each.
                opponent.rows.push_back(
                    {"tier", std::to_string(driverData->tier) + " (0 easy .. 2 hard)"});
                opponent.rows.push_back(
                    {"reaction", std::to_string(driverData->reaction) + " (0x0a best)"});
                opponent.rows.push_back(
                    {"aggression", std::to_string(driverData->aggression) + " (0x04 best)"});
            } else {
                opponent.rows.push_back({"stats", "no SR_data row"});
            }
        } else {
            opponent.rows.push_back({"driver", "none"});
        }

        for (const DebugRow& row : racing->GetOpponentDebugRows()) {
            opponent.rows.push_back(row);
        }
        opponent.indicators = racing->GetOpponentIndicators();
        panels.push_back(std::move(opponent));
    }
    return panels;
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
    // Blood goes on top of the wrecked interior, and only on a hard enough hit.
    if (crashBloodBitmap && racing && racing->ShouldShowCrashBlood()) {
        crashBloodBitmap->SetVisible(true);
        crashBloodBitmap->Draw();
    }
}

void Driving::DrawSteeringDot() {
    if (!racing || racing->IsFlybyMode() || racing->ShouldShowCrashOverlay()) {
        return;
    }

    // The dot rides the top of the steering-wheel rim: twelve o'clock when
    // centered, rotating with the eased steering position so it swings out as you
    // turn and drifts back to center over a few frames on release.
    //
    // Geometry is in the 320x200 dashboard space (same as the gauge needles).
    // TUNE these four to sit the dot on your wheel rim.
    constexpr float kWheelCx = 171.0f;     // wheel center X (between the gauges)
    constexpr float kWheelCy = 182.0f;     // wheel center Y
    constexpr float kWheelR = 82.0f;       // rim radius the dot travels on
    constexpr float kRadPerUnit = 0.045f;  // steer units -> radians (max ~ +-0.8)
    constexpr float kDotR = 1.5f;               // dot radius in pixels

    const float steer = static_cast<float>(racing->GetSteerDisplay());
    const float theta = steer * kRadPerUnit;  // +steer (right) swings the dot right
    const int dx = static_cast<int>(std::lround(kWheelCx + kWheelR * std::sin(theta)));
    const int dy = static_cast<int>(std::lround(kWheelCy - kWheelR * std::cos(theta)));

    SDL_SetRenderDrawColor(renderer, 220, 30, 30, 255);
    for (int y = -kDotR; y <= kDotR; ++y) {
        for (int x = -kDotR; x <= kDotR; ++x) {
            if (x * x + y * y <= kDotR * kDotR) {
                SDL_RenderDrawPoint(renderer, dx + x, dy + y);
            }
        }
    }
}

void Driving::OnSceneExit() {
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (am && !musicWasPaused) {
        am->ResumeMusic();
    }
    Scene::OnSceneExit();
}
