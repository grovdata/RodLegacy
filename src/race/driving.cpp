#include "core/game_text.h"
#include "core/log.h"
#include "race/driving.h"
#include "core/state.h"
#include "scenes/grudge_night.h"
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

extern Mouse mouse;

// Racing paints the door mirror into the road surface, so make the glass transparent.
// Bitmap y is screen y - 100.
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
    racing = std::make_unique<Racing>(state.GetAssets().GetAudioManager());
    roadTexture = nullptr;
    lastEvent = nullptr;
    
    trackReader = std::make_unique<TrackReader>();
    LOG_INFO("Driving constructor completed - trackReader created\n");
}

void Driving::Init(bool debugMode) {
    LOG_INFO("Driving::Init() called with debugMode=%s\n", debugMode ? "true" : "false");
    Scene::Init(debugMode);

    raceEnded = false;
    raceEndPendingKing = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;

    ApplyCarLivery();

    Bitmap* dashboard = dynamic_cast<Bitmap*>(GetDrawable("dashboard"));

    if (state.GetAssets().GetGame() == SrGame::SR2) {
        LoadSr2Cockpit();
        LoadShifter();
        SetState(DRIVING_STATE_NORMAL);
        LoadTrackData();
        if (racing) racing->SetDebugFeaturesEnabled(debugMode);
        return;
    }

    auto pillar = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WINDSHIELD_BEAM});
    pillar->AlignAbove(dashboard);
    pillar->SetVisible(true);
    AddDrawable("pillar", pillar);

    auto roof = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CEILING});
    roof->AlignAbove(pillar);
    roof->SetVisible(true);
    AddDrawable("roof", roof);

    auto mirror = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_REARVIEWMIRROR_FRAME});
    mirror->SetPosition(240, 19);
    mirror->SetVisible(true);
    AddDrawable("mirror", mirror);
    
    auto clock = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_CLOCK_11_50});
    clock->SetPosition(120, 115);
    clock->SetVisible(false);
    AddDrawable("clock", clock);

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

    wheelRightHand1 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_1});
    wheelRightHand1->SetPosition(216, 144);
    wheelRightHand1->SetVisible(true);
    AddWheelDrawable("wheel_right_hand_1", wheelRightHand1);

    wheelRightHand2 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_2});
    wheelRightHand2->SetPosition(216, 120);
    wheelRightHand2->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_2", wheelRightHand2);

    wheelRightHand3 = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_3});
    wheelRightHand3->SetPosition(216, 144);
    wheelRightHand3->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_3", wheelRightHand3);

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
    
    // Not car-coloured, so the plain driving palette.
    const bool sr2Blood = state.GetAssets().GetGame() == SrGame::SR2;
    auto* crashBloodAsset = state.GetAssets().GetBitmapAsset(
        sr2Blood ? Asset{LibAssetRegular, SrLibFileAmigaLibP, SR2_RACING_CRASHED_BLOOD, SrGame::SR2}
                 : Asset{LibAssetRegular, SrLibFileAmigaLibP, RACING_CRASHED_BLOOD});
    if (crashBloodAsset) {
        crashBloodBitmap = std::make_unique<Bitmap>(*crashBloodAsset);
        crashBloodBitmap->SetVisible(false);
        crashBloodBitmap->SetPosition(sr2Blood ? 128 : 104, 27);
    }
    
    LoadTrackData();

    if (racing) {
        racing->SetDebugFeaturesEnabled(debugMode);
    }
}

void Driving::LoadTrackData() {
    LOG_INFO("Driving::LoadTrackData() called\n");
    
    if (!trackReader) {
        LOG_INFO("ERROR: trackReader is null!\n");
        return;
    }
    
    byte_vector& srExeData = state.GetAssets().GetSrExeData();
    LOG_INFO("AssetManager SrExeData size: %zu bytes\n", srExeData.size());
    
    if (srExeData.empty()) {
        LOG_INFO("ERROR: AssetManager SrExeData is empty!\n");
        return;
    }
    
    if (trackReader->LoadTrackData(srExeData)) {
        LOG_INFO("=== Street Rod Track Data Loaded ===\n");
        
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
        
        LOG_INFO("Extracting roadside sprites...\n");
        auto roadsideSprites = trackReader->ExtractRoadSideSprites(SR_ROADSIDE_SPRITES_OFFSET, 100);
        LOG_INFO("Found %zu roadside sprite records\n", roadsideSprites.size());
        if (!roadsideSprites.empty()) {
            LOG_INFO("Roadside Sprites: %zu records\n", roadsideSprites.size());
            
            for (size_t i = 0; i < std::min<size_t>(5, roadsideSprites.size()); ++i) {
                const auto& sprite = roadsideSprites[i];
                LOG_INFO("  %s at pos %d, %s at pos %d\n",
                    TrackReader::GetSpriteName(sprite.spriteId1), sprite.trackPosition1,
                    TrackReader::GetSpriteName(sprite.spriteId2), sprite.trackPosition2);
            }
        }
        
        if (racing) {
            LOG_INFO("Loading track data into racing component...\n");
            // Builds the race sim off the current car, gearbox included.
            racing->LoadTrackData(srExeData, SR_TRACK_ROAD_RACE);

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
    if (roadTexture) {
        SDL_DestroyTexture(roadTexture);
        roadTexture = nullptr;
    }
}

// Init runs once at game start, so this re-runs per race.
void Driving::ApplyCarLivery() {
    Car* car = state.GetCurrentCar();
    if (!car) return;

    // The copperlist loads the hood's COLOR06/COLOR07 with shades 2 and 3 of the car's ramp. Palette slot N maps
    // to carPalette[N-1], so they sit at 5 and 6.
    std::vector<uint16_t> hoodPalette = GetStandardCarColor(car->GetColor());
    if (hoodPalette.size() >= 7) {
        hoodPalette[5] = hoodPalette[2];
        hoodPalette[6] = hoodPalette[3];
    }

    byte_vector& libFile = state.GetAssets().GetLibPData();

    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    
    // The dashboard includes the hood's left edge.
    Bitmap* dashboard = sr2
        ? state.GetAssets().GetBitmapAssetWithPalette(
              state.GetAssets().GetLibPData(SrGame::SR2),
              {LibAssetRegular, SrLibFileAmigaLibP,
               kSr2Interiors[std::clamp(car->GetModel().sprites.dashboard, 0, 2)].dashboard, SrGame::SR2},
              hoodPalette)
        : state.GetAssets().GetBitmapAssetWithPalette(
              libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_INTERIOR_1}, hoodPalette);
    if (!sr2) PunchSideMirrorGlass(dashboard);
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);
    SetDrawable("dashboard", dashboard);

    if (sr2) {
        // The tall hood belongs to body style 3, the low one to every other car; both end where the dashboard starts.
        const int hoodId = car->GetModel().sprites.bodyStyle == 3 ? SR2_HOOD_TALL : SR2_HOOD_LOW;
        Bitmap* sr2Hood = state.GetAssets().GetBitmapAssetWithPalette(
            state.GetAssets().GetLibPData(SrGame::SR2),
            {LibAssetRegular, SrLibFileAmigaLibP, hoodId, SrGame::SR2}, hoodPalette);
        sr2Hood->SetPosition(72, dashboard->GetPosition().y - sr2Hood->GetNaturalSize().h);
        sr2Hood->SetVisible(true);
        SetDrawable("hood", sr2Hood);
        // The wrecked interior shows the smashed window, in the car's hood shades like SR's.
        Bitmap* crashOverlayAsset = state.GetAssets().GetBitmapAssetWithPalette(
            state.GetAssets().GetLibPData(SrGame::SR2),
            {LibAssetRegular, SrLibFileAmigaLibP, SR2_CRASH_WINDOW_SMASHED, SrGame::SR2}, hoodPalette);
        if (crashOverlayAsset) {
            crashOverlayBitmap = std::make_unique<Bitmap>(*crashOverlayAsset);
            crashOverlayBitmap->SetVisible(false);
            crashOverlayBitmap->SetPosition(48, 18);   // 272 wide: it fills the windshield from the left pillar to the edge
        }
        return;
    }

    Bitmap* hood = state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD}, hoodPalette);
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    SetDrawable("hood", hood);

    // The wrecked interior shows the buckled hood.
    Bitmap* crashOverlayAsset = state.GetAssets().GetBitmapAssetWithPalette(
        libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_CRASHED_INTERIOR}, hoodPalette);
    if (crashOverlayAsset) {
        crashOverlayBitmap = std::make_unique<Bitmap>(*crashOverlayAsset);
        crashOverlayBitmap->SetVisible(false);
        crashOverlayBitmap->SetPosition(0, 18);
    }
}

// Where each piece sits in the 320x90 dashboard bitmap; the grips are the neutral hand positions.
static constexpr SDL_Point kSr2GripLeft = {67, 51};
static constexpr SDL_Point kSr2GripRight = {241, 51};
// Turning brings one arm in over the wheel (cells cut at 0x239e02 and 0x239e7a).
static constexpr SDL_Point kSr2ArmLeft = {29, 31};
static constexpr SDL_Point kSr2ArmRight = {229, 30};

void Driving::LoadSr2Cockpit() {
    auto* dash = dynamic_cast<Bitmap*>(GetDrawable("dashboard"));
    const SDL_Rect dashRect = dash ? dash->GetPosition() : SDL_Rect{0, 0, 0, 0};
    Car* car = state.GetCurrentCar();
    const Sr2Interior& in = kSr2Interiors[std::clamp(car ? car->GetModel().sprites.dashboard : 0, 0, 2)];

    auto get = [](int id) {
        return state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, id, SrGame::SR2});
    };

    auto roof = get(SR2_RACING_CEILING);
    roof->SetPosition(0, 0);
    roof->SetVisible(true);
    SetDrawable("roof", roof);

    auto pillarLeft = get(car && car->GetModel().sprites.bodyStyle == 3 ? SR2_RACING_PILLAR_TALL_HOOD : SR2_RACING_PILLAR_LOW_HOOD);
    pillarLeft->SetPosition(0, 18);
    pillarLeft->SetVisible(true);
    SetDrawable("pillar_left", pillarLeft);

    auto mirror = get(SR2_RACING_REARVIEWMIRROR_FRAME);
    mirror->SetPosition(240, 19);
    mirror->SetVisible(true);
    SetDrawable("mirror", mirror);

    auto clock = get(in.clock);
    clock->SetPosition(in.clockX, dashRect.y + in.clockY);
    clock->SetVisible(false);
    SetDrawable("clock", clock);

    const int wheels[3] = {in.wheelLeft, in.wheelNeutral, in.wheelRight};
    const char* names[3] = {"wheel_left", "wheel_neutral", "wheel_right"};
    for (int i = 0; i < 3; ++i) {
        sr2Wheel[i] = get(wheels[i]);
        sr2Wheel[i]->SetPosition(SR2_WHEEL_X, dashRect.y + SR2_WHEEL_Y);
        sr2Wheel[i]->SetVisible(i == 1);
        SetDrawable(names[i], sr2Wheel[i]);
    }

    sr2Arm[0] = get(SR2_ARM_LEFT);
    sr2Arm[0]->SetPosition(kSr2ArmLeft.x, dashRect.y + kSr2ArmLeft.y);
    sr2Arm[0]->SetVisible(false);
    SetDrawable("arm_left", sr2Arm[0]);

    sr2Arm[1] = get(SR2_ARM_RIGHT);
    sr2Arm[1]->SetPosition(kSr2ArmRight.x, dashRect.y + kSr2ArmRight.y);
    sr2Arm[1]->SetVisible(false);
    SetDrawable("arm_right", sr2Arm[1]);

    sr2Grip[0] = get(SR2_HAND_GRIP_LEFT);
    sr2Grip[0]->SetPosition(kSr2GripLeft.x, dashRect.y + kSr2GripLeft.y);
    sr2Grip[0]->SetVisible(true);
    SetDrawable("grip_left", sr2Grip[0]);

    sr2Grip[1] = get(SR2_HAND_GRIP_RIGHT);
    sr2Grip[1]->SetPosition(kSr2GripRight.x, dashRect.y + kSr2GripRight.y);
    sr2Grip[1]->SetVisible(true);
    SetDrawable("grip_right", sr2Grip[1]);
}

// Turning brings one arm in over the wheel and hides both hands.
void Driving::SetSr2State(DrivingState newState) {
    currentState = newState;
    sr2Wheel[0]->SetVisible(newState == DRIVING_STATE_TURNING_LEFT);
    sr2Wheel[1]->SetVisible(newState != DRIVING_STATE_TURNING_LEFT && newState != DRIVING_STATE_TURNING_RIGHT);
    sr2Wheel[2]->SetVisible(newState == DRIVING_STATE_TURNING_RIGHT);
    const bool shifting = shiftPhase != ShiftPhase::Idle;
    const bool left = newState == DRIVING_STATE_TURNING_LEFT;
    const bool right = newState == DRIVING_STATE_TURNING_RIGHT;
    sr2Arm[0]->SetVisible(right);
    sr2Arm[1]->SetVisible(left);
    sr2Grip[0]->SetVisible(!left && !right);
    sr2Grip[1]->SetVisible(!shifting && !left && !right);
}

void Driving::AddWheelDrawable(const std::string& id, Bitmap* bitmap) {
    AddDrawable(id, bitmap);
}

void Driving::Start() {
}

void Driving::SetState(DrivingState state) {
    if (sr2Wheel[1]) {
        SetSr2State(state);
        return;
    }
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
            break;
    }

    // The right hand is on the shifter during a shift; steering can re-run SetState mid-shift.
    if (shiftPhase != ShiftPhase::Idle) {
        wheelRightHand1->SetVisible(false);
        wheelRightHand2->SetVisible(false);
        wheelRightHand3->SetVisible(false);
        wheelRightHand4->SetVisible(false);
    }
}

void Driving::OnSceneEnter() {
    // Init() runs once for every scene, so pausing music there would kill it for the session.
    if (AudioManager* am = state.GetAssets().GetAudioManager()) {
        musicWasPaused = am->IsMusicPaused();
        am->PauseMusic();
    }

    if (!racing) return;

    // Before the track loads, which builds the race sim off the current car.
    ApplyCarLivery();
    if (sr2Wheel[1]) LoadSr2Cockpit();   // the car may have a different dashboard

    RaceType pendingType = state.GetPendingRaceType();
    SrTrackType trackType = (pendingType == RaceType::Drag)
                            ? SR_TRACK_DRAG_RACE : SR_TRACK_ROAD_RACE;
    // The Aqueduct is not one of the executable's two tracks.
    racing->SetAqueductRace(pendingType == RaceType::Aqueduct);

    byte_vector& srExeData = state.GetAssets().GetSrExeData();
    if (!srExeData.empty()) {
        racing->LoadTrackData(srExeData, trackType);
    } else {
        racing->SetTrackType(trackType);
    }

    // Init() runs once, so per-race scene state is cleared here.
    raceEnded = false;
    raceEndPendingKing = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;
    wreckDialog.reset();
    raceEndDismissAllowedMs = 0;
    mouse.SetVisible(false);

    currentDiceIndex = 0;
    diceTilt = 0;
    diceDir = 1;
    diceStepTimer = 0.0f;

    racing->ResetForNewRace();
    racing->InitializeOpponent();

    // After ResetForNewRace: the gate plate depends on this car's box.
    ResetShifter();

    // A drive arrives as SCENE_CRUISING with the destination in State. Last, so the reset cannot undo it.
    cruising = false;
    SrLibGameScene cruiseTo = SCENE_DINER;
    if (state.TakePendingCruise(cruiseTo)) {
        StartCruise(CruiseStartSegment(cruiseTo), cruiseTo);
        LOG_INFO("Driving::OnSceneEnter - cruising to scene %d", static_cast<int>(cruiseTo));
        return;
    }

    LOG_INFO("Driving::OnSceneEnter - race starting: type=%s bet=%d pinkSlip=%d",
             pendingType == RaceType::Drag ? "Drag"
                 : pendingType == RaceType::Aqueduct ? "Aqueduct"
                 : pendingType == RaceType::Grudge ? "Grudge" : "Road",
             state.GetPendingBetAmount(),
             state.GetPendingIsPinkSlip() ? 1 : 0);
}

// The knob walks a gate path one stop per race frame with a hand on it, bare at each end. Offsets from the
// plate's top-left; the manual H-pattern's odd entries route through the gate.
static constexpr SDL_Point kGateManual[9] = {
    {22, 19}, {22, 11}, {22, 19}, {22, 29}, {22, 19},
    {31, 19}, {31, 11}, {31, 19}, {31, 29}
};
static constexpr SDL_Point kGateAuto[3] = {{28, 21}, {28, 27}, {28, 34}};
// Where each gear rests on the manual path; an automatic uses 0 (N) and 2 (D).
static constexpr int kGearStop[5] = {0, 1, 3, 6, 8};

void Driving::LoadShifter() {
    const bool sr2 = state.GetAssets().GetGame() == SrGame::SR2;
    const SrGame game = sr2 ? SrGame::SR2 : SrGame::SR1;
    // SR2's plates 71-73 follow SR1's order: automatic, 3-speed, 4-speed.
    const int plates[3] = {sr2 ? 71 : RACING_GEARBOX_AUTO, sr2 ? 72 : RACING_GEARBOX_MANUAL_3SPEED,
                           sr2 ? 73 : RACING_GEARBOX_MANUAL_4SPEED};
    const int knobs[3] = {sr2 ? SR2_RACING_GEARBOX_KNOB_AUTOMATIC : RACING_GEARBOX_KNOB_AUTOMATIC,
                          sr2 ? SR2_RACING_GEARBOX_KNOB_3SPEED : RACING_GEARBOX_KNOB_3SPEED,
                          sr2 ? SR2_RACING_GEARBOX_KNOB_4SPEED : RACING_GEARBOX_KNOB_4SPEED};
    for (int i = 0; i < 3; ++i) {
        shifterPlate[i] = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, plates[i], game});
        shifterPlate[i]->SetVisible(false);
        AddDrawable("shifter_plate_" + std::to_string(i), shifterPlate[i]);
        shifterKnob[i] = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, knobs[i], game});
        shifterKnob[i]->SetVisible(false);
        AddDrawable("shifter_knob_" + std::to_string(i), shifterKnob[i]);
    }
    shifterHand = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP,
                                                    sr2 ? SR2_RACING_RIGHT_HAND_SHIFTING : RACING_RIGHT_HAND_SHIFTING, game});
    shifterHand->SetVisible(false);
    AddDrawable("shifter_hand", shifterHand);

    // The plate sits at (0x110, 0x2c) in SR's bottom-aligned dashboard bitmap, (0x112, 0x33) in SR2's.
    auto* dash = dynamic_cast<Bitmap*>(GetDrawable("dashboard"));
    SDL_Rect dashRect = dash ? dash->GetPosition() : SDL_Rect{0, 0, 0, 0};
    shifterOrigin = {dashRect.x + (sr2 ? 0x112 : 0x110), dashRect.y + (sr2 ? 0x33 : 0x2c)};
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

    const bool active = (shiftPhase != ShiftPhase::Idle);
    shifterPlate[shifterKind]->SetVisible(active);
    shifterKnob[shifterKind]->SetVisible(active && shiftPhase != ShiftPhase::Slide);
    shifterHand->SetVisible(shiftPhase == ShiftPhase::Slide);
    if (!active) return;

    if (shiftPhase == ShiftPhase::Slide) {
        // Top-left at (0xfc, 0x22): the arm reaches in from there.
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
    // A continuous pendulum: tilt -2..2 steps each race frame, reversing at its bounds. Steering shifts one bound;
    // stopped, it settles once it crosses centre.
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

    // -2 LEFT2(2), -1 LEFT1(1), 0 NEUTRAL(0), +1 RIGHT1(3), +2 RIGHT2(4).
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

    if (racing) {
        // Frozen while the race-end bubble is up.
        if (!raceEnded) {
            float deltaTime = state.GetElapsedTime() / 1000.0f;
            if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap at 100ms to prevent huge jumps

            racing->Draw(deltaTime, lastEvent);

            if (cruising) {
                if (racing->CruiseFinished()) {
                    EndCruise();
                    return;
                }
            } else if (!racing->IsFlybyMode()) {
                const RaceOutcome outcome = racing->GetRaceOutcome();
                if (outcome != RaceOutcome::Ongoing) SettleRace(outcome);
            }

            UpdateDiceAnimation(deltaTime);

            UpdateShifter(deltaTime);
        }

        SDL_Surface* racingSurface = racing->GetSurface();
        if (racingSurface) {
            if (roadTexture) {
                SDL_DestroyTexture(roadTexture);
            }
            
            roadTexture = SDL_CreateTextureFromSurface(renderer, racingSurface);
            if (roadTexture) {
                // The scale-quality hint is set after the renderer exists, so it isn't reliably applied.
                SDL_SetTextureScaleMode(roadTexture, SDL_ScaleModeNearest);
                SDL_RenderCopy(renderer, roadTexture, NULL, &roadRect);
            }
        }
    }

    bool isAutomatic = racing && racing->IsAutomatic();
    
    auto clockDrawable = GetDrawable("clock");
    if (clockDrawable) {
        clockDrawable->SetVisible(isAutomatic);
    }
    
    // Follows the eased steering, so it re-centers over several frames.
    if (racing && !racing->IsFlybyMode() && !racing->ShouldShowCrashOverlay()) {
        const int steer = racing->GetSteerDisplay();
        DrivingState wheelState = (steer > 0) ? DRIVING_STATE_TURNING_RIGHT
                                : (steer < 0) ? DRIVING_STATE_TURNING_LEFT
                                              : DRIVING_STATE_NORMAL;
        if (wheelState != currentState) {
            SetState(wheelState);
        }
    }

    DrawAll();
    
    if (racing) {
        if (!isAutomatic) {
            racing->DrawTachometerNeedleToRenderer(renderer);
        }
        racing->DrawSpeedometerNeedleToRenderer(renderer);
    }

    DrawSteeringDot();

    bool isCrashed = racing && !racing->IsFlybyMode() && racing->ShouldShowCrashOverlay();
    
    if (isCrashed != lastCrashState) {
        if (isCrashed) {
            SetState(DRIVING_STATE_CRASHED);
        } else {
            SetState(DRIVING_STATE_NORMAL);
        }
        lastCrashState = isCrashed;
    }
    
    if (isCrashed) {
        DrawCrashOverlay();
    } else if (crashOverlayBitmap && crashOverlayBitmap->IsVisible()) {
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
    if (raceEnded && raceEndBubble) {
        raceEndBubble->Draw();
    }
    if (wreckDialog && wreckDialog->IsVisible()) {
        wreckDialog->Draw();
    }
}

// The race view fades out and back in before the message.
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
    // ProcessRaceOutcome: beating the King on the road starts the victory piece under the result, which runs on
    // into the ending.
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (am && raceEndPendingKing && state.GetAssets().GetGame() != SrGame::SR2) {
        am->PlayVictoryMusic();
    } else if (am && !musicWasPaused) {
        am->ResumeMusic();
    }
    ShowRaceEndMessage();
}

// Dialog 24: two lines of race stats, no buttons. The original's backticks render as spaces.
void Driving::ShowStatsDialog() {
    std::string line1;
    std::string line2;
    if (raceEndWon) {
        line1 = GameText::Format(GameText::WINNING_TIME, raceEndTimeTenths / 10, raceEndTimeTenths % 10,
                                 raceEndAvgSpeed);
        line2 = GameText::Get(GameText::CONGRATULATIONS);
    } else {
        line1 = GameText::Format(GameText::COVERED_ONLY, raceEndTimeTenths / 10, raceEndTimeTenths % 10,
                                 raceEndPercent);
        line2 = GameText::Format(GameText::OF_DISTANCE, raceEndAvgSpeed);
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
    } else if (!raceEndReason.text.empty()) {
        BubbleStyle style = BUBBLE_LEFT;
        switch (raceEndReason.box) {
            case srrace::MessageBox::Narrow: style = BUBBLE_RIGHT; break;
            case srrace::MessageBox::Wide:   style = BUBBLE_LEFT; break;
            case srrace::MessageBox::TwoRow: style = BUBBLE_RIGHT_TWO_LINES; break;
        }
        raceEndBubble = std::make_unique<Bubble>(raceEndReason.text, style, false);
        raceEndBubble->SetVisible(true);
    }
    // So a still-held accelerator can't skip the message.
    raceEndDismissAllowedMs = SDL_GetTicks() + 4000;
    mouse.SetPointerType(raceEndWon ? SrLibAmigaSpriteAssetMousePointerThumbsUp
                                    : SrLibAmigaSpriteAssetMousePointerThumbsDown);
    mouse.SetVisible(true);
}

// Above 60 mph the quote is worded as a parts payout. "Fix it" is absent when the player can't afford it.
void Driving::ShowWreckDialog(const srrace::RaceEndContext& ctx) {
    std::uniform_int_distribution<int> roll(0, srrace::RepairRollRange(ctx.crashSpeed) - 1);
    const int cost = 100 + roll(RandomEngine());
    const int junk = srrace::JunkPayout(ctx.carValue);
    const bool affordable = cost <= state.GetBankroll();

    const std::string quote = GameText::Get(ctx.crashSpeed > 60 ? GameText::REPAIR_PARTS : GameText::REPAIR_COST, cost);

    SDL_Rect dialogRect = {34, 90, 252, 56};
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{38, 94, 248, 10}, GameText::Get(GameText::COULD_BE_WORSE),
                         {0x00, 0x00, 0x00, 0xFF}, true},
        DialogTextConfig{SDL_Rect{38, 104, 248, 10}, quote, {0x00, 0x00, 0x00, 0xFF}, true}
    };
    if (ctx.tiresBlown) {
        texts.push_back(DialogTextConfig{SDL_Rect{38, 114, 248, 10}, GameText::Get(GameText::PLUS_TIRES),
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
    // The race hid the pointer and the scene is not re-entered.
    mouse.SetVisible(true);
    mouse.JumpToClosestZone(this);
}

// The sim wears a copy of the setup, so its wear is written back here.
void Driving::ApplyRaceWear() {
    Car* car = state.GetCurrentCar();
    if (!car || !racing) return;
    car->AddEngineWearRaw(racing->PlayerEngineWearDelta());
    car->AddGearboxWearRaw(racing->PlayerTransWearDelta());

    // Tires: one lump at the end, only if the car passed segment 200.
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

// The original fits a new set on the spot: a spare if there is one, otherwise $50 (bankroll floored at zero).
// Returns what is still owed.
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

// Settles a finished race: stake, reputation, fuel, clock, wear, tires, fine, and whether the evening
// is over. Then starts the race-end screen.
void Driving::SettleRace(RaceOutcome outcome) {
    const bool won = (outcome == RaceOutcome::PlayerWon);
    // The track type only tells Drag from Road; Grudge and Aqueduct come from the pending type.
    RaceType raceType = state.GetPendingRaceType();
    if (raceType != RaceType::Grudge && raceType != RaceType::Aqueduct) {
        raceType = (racing->GetTrackType() == SR_TRACK_DRAG_RACE) ? RaceType::Drag : RaceType::Road;
    }
    const bool wasKingRace = (state.GetLastChallengerPoolIndex() == RACER_KING_SLOT);
    srrace::RaceEndContext endCtx = racing->BuildRaceEndContext();
    endCtx.vsKing = wasKingRace;
    const srrace::RaceEndMessage reason = srrace::BuildRaceEndReason(endCtx);
    const srrace::CrashOutcome crash = srrace::ClassifyCrash(endCtx);
    // Read before ResetRaceOutcome clears it; charged after the tires, whose debt is added to the fine.
    const bool busted = racing->PlayerBusted();
    const int speedingFine = busted ? racing->SpeedingFine() : 0;

    // A ticket or a jumped light still counts the race, but leaves the stake alone.
    raceEndPendingKing = state.OnRaceFinished(raceType, won, srrace::SettlesBet(endCtx));
    // A grudge round reports back to the night, whose verdict replaces the race-end line.
    if (raceType == RaceType::Grudge && state.GetGrudgeNight() != nullptr) {
        const bool failed = (endCtx.endFlags & srrace::END_FAILURE_MASK) != 0;
        const bool crossedLine = (endCtx.endFlags & srrace::END_CROSSED_LINE) != 0;
        state.GetGrudgeNight()->OnRaceFinished(won, failed, racing->RaceTimeTenths(),
                                               racing->LossMarginTenths(), crossedLine);
        grudgeResultLine = state.GetGrudgeNight()->ResultMessage();
    }
    racing->ResetRaceOutcome();
    // On top of the drive legs State::MoveToScene charges.
    if (state.GetAssets().GetGame() == SrGame::SR2) {
        if (Car* car = state.GetCurrentCar()) car->SetSr2Tune(Sr2DriftTune(car->GetSr2Tune(), RandomEngine()));
    }
    state.BurnFuel(
        raceType == RaceType::Drag     ? State::FUEL_PER_DRAG_RACE_TENTHS
      : raceType == RaceType::Aqueduct ? State::FUEL_PER_AQUEDUCT_RACE_TENTHS
                                       : State::FUEL_PER_ROAD_RACE_TENTHS);
    // King races advance the clock 4h (drag), 5h (Aqueduct) or 7h instead of 2h. Street Rod 2 charges that for
    // every race it starts from the drive-in (ldrivein.c 0x25092a: 600 / 750 / 1050 ticks).
    const bool sr2Race = state.GetAssets().GetGame() == SrGame::SR2 && raceType != RaceType::Grudge;
    if (state.GetAssets().GetGame() == SrGame::SR2 && raceType == RaceType::Grudge) {
        // lgrudge.c never touches the clock; the trip there and back is charged by the garage.
    } else if (wasKingRace || sr2Race) {
        state.AdvanceGameHours(raceType == RaceType::Drag ? 4 : raceType == RaceType::Aqueduct ? 5 : 7);
    } else {
        state.AdvanceGameHours(2);
    }

    ApplyRaceWear();
    // What a fresh set left unpaid if the $50 could not be covered.
    int tireDebt = 0;
    if (endCtx.tiresBlown) {
        tireDebt = ApplyBlownTires();
    }
    ApplyDrivetrainDamage(endCtx.endFlags);

    // Tires first, then the fine; any unpaid tire debt is billed on top of the ticket.
    if (busted) {
        const int owed = speedingFine + tireDebt;
        if (state.GetBankroll() >= owed) {
            state.SetBankroll(state.GetBankroll() - owed);
        } else {
            // Short of it: jail, and the run is over.
            state.SetJailed(true);
        }
    }

    // Any wreck, a failed drivetrain, unpaid tires or a settled pink slip send the player home.
    const bool drivetrainFailed =
        (endCtx.endFlags &
         (srrace::END_ENGINE_BLOWN | srrace::END_TRANS_DROPPED)) != 0;
    raceEndLeftTheDiner = (crash == srrace::CrashOutcome::Totaled ||
                           crash == srrace::CrashOutcome::RepairOrJunk ||
                           crash == srrace::CrashOutcome::DroveAway ||
                           crash == srrace::CrashOutcome::Forfeited ||
                           drivetrainFailed ||
                           tireDebt > 0 ||
                           (endCtx.isPinkSlip && srrace::SettlesBet(endCtx)));

    // After the bet is settled.
    if (crash == srrace::CrashOutcome::Totaled) {
        const int scrap = srrace::ScrapPayout(endCtx.carStrippedValue);
        state.SetBankroll(state.GetBankroll() + scrap);
        state.ScrapCurrentCar();
        LOG_INFO("Driving: car totaled at %d mph, $%d for scrap",
                 endCtx.crashSpeed, scrap);
    }

    LOG_INFO("Driving: race ended (%s) endFlags=0x%04x seg=%d avgSpeed=%d crashSpeed=%d; press any key for garage",
             reason.text.c_str(), (unsigned)endCtx.endFlags, (int)racing->PlayerSegment(),
             racing->AverageSpeedMph(), endCtx.crashSpeed);
    raceEnded = true;
    // ShowRaceEndMessage puts the message up after the dip.
    raceEndReason = reason;
    if (!grudgeResultLine.empty()) {
        raceEndReason = {grudgeResultLine, srrace::MessageBox::TwoRow};
        grudgeResultLine.clear();
    }
    raceEndWon = won;
    raceEndWreck = (crash == srrace::CrashOutcome::RepairOrJunk);
    raceEndCtx = endCtx;
    // Only for a race decided at the line, and not on Grudge Night, which reports a margin instead.
    raceEndShowStats = (endCtx.endFlags & srrace::END_FAILURE_MASK) == 0 && !endCtx.isGrudge;
    raceEndTimeTenths = racing->RaceTimeTenths();
    raceEndAvgSpeed = racing->AverageSpeedMph();
    raceEndPercent = racing->DistancePercent();
    raceEndDipping = true;
    FadeOut();
}

void Driving::ApplyDrivetrainDamage(uint16_t endFlags) {
    Car* car = state.GetCurrentCar();
    if (!car) return;
    if (endFlags & (srrace::END_ENGINE_BLOWN | srrace::END_TRANS_DROPPED)) {
        if (AudioManager* am = state.GetAssets().GetAudioManager()) am->PlaySound(AudioSampleType::DRIVETRAIN_BLOWN);
    }
    if (endFlags & srrace::END_ENGINE_BLOWN) {
        car->StripEngineAssembly();
        LOG_INFO("Driving: engine blown - engine, manifold and carburetors stripped");
    } else if (endFlags & srrace::END_TRANS_DROPPED) {
        car->RemoveGearbox();
        LOG_INFO("Driving: transmission dropped - gearbox gone");
    }
}

// The original starts the drive to the garage, diner and gas station at 10, 65 and 115.
int Driving::CruiseStartSegment(SrLibGameScene destination) {
    if (destination == SCENE_DINER) {
        return 65;
    }
    if (destination == SCENE_GAS_STATION) {
        return 115;
    }
    return 10;   // the garage, and anything else that drives
}

void Driving::StartCruise(int startSeg, SrLibGameScene destination) {
    cruising = true;
    cruiseDestination = destination;
    raceEnded = false;
    raceEndBubble.reset();
    statsDialog.reset();
    raceEndDipping = false;
    mouse.SetVisible(false);
    // The original shows this on the cutscene's first tick.
    hud->SetTextOnly("Cruising in town...");
    if (racing) {
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
    hud->Clear();
    state.ArriveFromCruise(cruiseDestination);
}

// Called from the wreck dialog's own callback, so it is hidden, not destroyed.
void Driving::LeaveAfterRace() {
    raceEnded = false;
    raceEndBubble.reset();
    statsDialog.reset();
    if (wreckDialog) wreckDialog->Hide();
    const bool endGame = raceEndPendingKing;
    raceEndPendingKing = false;
    if (endGame) {
        state.MoveToScene(SCENE_ENDING);
    } else if (state.GetPendingRaceType() == RaceType::Grudge) {
        // The night runs its own rounds, so a grudge round goes back to it
        // rather than out to the diner or the garage.
        state.MoveToScene(SCENE_GRUDGE_NIGHT);
    } else if (state.IsJailed()) {
        // Not the garage: you are in a cell, watching the diner.
        state.MoveToScene(SCENE_DINER);
    } else if (state.GetCurrentCar() == nullptr || raceEndLeftTheDiner) {
        // The garage, and the out-of-dough check that lives in its loop
        // (Garage::OnSceneEnter). With money left, the paper is a click away.
        state.MoveToScene(SCENE_GARAGE);
    } else {
        // Only ProcessRaceOutcome's 3 keeps the diner loop running.
        state.MoveToScene(SCENE_DINER);
    }
}

namespace {
// "Any key" includes the pad's buttons.
bool IsDismissEvent(const SDL_Event* e) {
    return e->type == SDL_KEYDOWN || e->type == SDL_CONTROLLERBUTTONDOWN;
}
}

bool Driving::PushEvent(SDL_Event *event) {
    // Any key dismisses the bubble, except the wreck dialog, which wants a click.
    // "You can skip the cruising by pressing the selector" (manual).
    if (cruising && (IsDismissEvent(event) || event->type == SDL_MOUSEBUTTONDOWN)) {
        EndCruise();
        return true;
    }

    if (raceEnded && !raceEndDipping && !wreckDialog && IsDismissEvent(event)) {
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

    // In flyby the camera readout replaces the generic lines.
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
        DebugPanel racingPanel{"Racing", {}};
        racingPanel.toggles.push_back(
            {"Invulnerable (no crash, no over-rev)", racing->DebugInvulnerableFlag()});
        // Also reachable from the race menu in Street Rod 2; this forces it
        // on for any race, including from a Street Rod 1 session.
        racingPanel.toggles.push_back(
            {"Aqueduct channel (preview)", &racing->AqueductPreviewRef()});
        racingPanel.rows = racing->GetRacingDebugRows(racing->GetLastVisibleRow());
        if (racing->AqueductPreviewRef()) {
            // The camera height the channel is projected from is not traced
            // yet, so it is dialled in here rather than guessed in code.
            racingPanel.rows.push_back(
                {"aqueduct cam height", std::to_string(racing->AqueductCameraHeightRef())});
            racingPanel.buttons.push_back(
                {"cam -10", [this]() { racing->AqueductCameraHeightRef() -= 10; }});
            racingPanel.buttons.push_back(
                {"cam +10", [this]() { racing->AqueductCameraHeightRef() += 10; }});
            racingPanel.buttons.push_back(
                {"cam reset", [this]() { racing->AqueductCameraHeightRef() = 1900; }});
        }
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

        DebugPanel opponent{"Opponent", {}};
        const Opponent* opp = state.GetCurrentOpponent();
        if (opp != nullptr) {
            opponent.rows.push_back({"driver", ""});
            opponent.rows.push_back({"name", opp->driver.name});

            Bitmap* portrait = LoadDriverPortrait(opp->driver);
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

void Driving::DrawCrashOverlay() {
    if (crashOverlayBitmap) {
        crashOverlayBitmap->SetVisible(true);
        crashOverlayBitmap->Draw();
    }
    if (crashBloodBitmap && racing && racing->ShouldShowCrashBlood()) {
        crashBloodBitmap->SetVisible(true);
        crashBloodBitmap->Draw();
    }
}

void Driving::DrawSteeringDot() {
    if (!racing || racing->IsFlybyMode() || racing->ShouldShowCrashOverlay()) {
        return;
    }

    // SR1 0x231ca0 / SR2 amigagr.c 0x234fec: two pixels across per steering unit, dropping down the rim by a table,
    // in the dashboard's own rastport (screen y 100). SR2's sits 4 lower. Drawn in pen 15 as four short lines:
    //   x .. x+1 / x-1 .. x+2 / x-1 .. x+2 / x .. x+1,  x = 2 * steer + 170
    static constexpr int kRimDrop[25] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 6, 6, 7, 8, 8, 10, 11, 11, 13};
    constexpr int kDashTop = 100;
    const int steer = racing->GetSteerDisplay();
    const int x = 2 * steer + 170;
    const int y = kDashTop + kRimDrop[std::min(std::abs(steer), 24)] +
                  (state.GetAssets().GetGame() == SrGame::SR2 ? 4 : 0);

    SDL_SetRenderDrawColor(renderer, 220, 30, 30, 255);
    SDL_RenderDrawLine(renderer, x, y, x + 1, y);
    SDL_RenderDrawLine(renderer, x - 1, y + 1, x + 2, y + 1);
    SDL_RenderDrawLine(renderer, x - 1, y + 2, x + 2, y + 2);
    SDL_RenderDrawLine(renderer, x, y + 3, x + 1, y + 3);
}

void Driving::OnSceneExit() {
    if (racing) {
        racing->StopEngineSound();
    }
    AudioManager* am = state.GetAssets().GetAudioManager();
    if (am && !musicWasPaused) {
        am->ResumeMusic();
    }
    Scene::OnSceneExit();
}
