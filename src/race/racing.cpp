#include "race/racing.h"
#include "debug/debug_ui.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "car/car.h"
#include "car/car_system.h"
#include "car/engine_part.h"
#include "core/gamepad.h"
#include "core/global.h"
#include "core/log.h"
#include "car/part.h"
#include "race/track_gen.h"
#include "core/state.h"
#include "race/racing_internal.h"

using namespace racing_internal;

namespace {

// The rpm the drive cutscene shifts at and then holds in top gear.
constexpr int kCruiseRpm = 4500;
// How far ahead the arriving car turns in. One segment shows no turn, twelve drifts across the road; tune here.
constexpr int kCruiseArrivalSegs = 3;
// Sim ticks a one-shot sample takes to play, rounded up.
int SampleTicks(uint32_t length, uint32_t rate) {
    return static_cast<int>((length * static_cast<uint32_t>(srrace::SimHz()) + rate - 1) / rate);
}

// One needle table shared by speedo and tacho: 82 entries of {x0, y0, x1, y1} signed bytes, inner end first,
// relative to the dial origin. The needle is drawn from these lines, not swept with trig.
constexpr size_t kNeedleTableOffset = 0x12430;
constexpr size_t kNeedleTableEntries = 82;

std::vector<NeedleLine> LoadNeedleTable(const byte_vector& exe) {
    const size_t needed = kNeedleTableOffset + kNeedleTableEntries * 4;
    if (exe.size() < needed) {
        LOG_ERROR("LoadNeedleTable: Street_Rod exe too small (%zu < %zu)", exe.size(), needed);
        return {};
    }
    std::vector<NeedleLine> table(kNeedleTableEntries);
    for (size_t i = 0; i < kNeedleTableEntries; ++i) {
        const uint8_t* e = exe.data() + kNeedleTableOffset + i * 4;
        table[i] = {static_cast<int8_t>(e[0]), static_cast<int8_t>(e[1]),
                    static_cast<int8_t>(e[2]), static_cast<int8_t>(e[3])};
    }
    // Guards against the wrong build: entry 0 and 81 are mirror stops, and no offset exceeds 24px.
    const NeedleLine& lo = table.front();
    const NeedleLine& hi = table.back();
    if (lo.x0 != -2 || lo.y0 != 4 || lo.x1 != -10 || lo.y1 != 13 ||
        hi.x0 != 3 || hi.y0 != 4 || hi.x1 != 11 || hi.y1 != 13) {
        LOG_ERROR("LoadNeedleTable: unexpected needle data at 0x%zx - gauges disabled",
                  kNeedleTableOffset);
        return {};
    }
    return table;
}
int ToRaceType(SrTrackType trackType) {
    return trackType == SR_TRACK_DRAG_RACE ? 0 : 1;
}

}

Racing::Racing() : rng(static_cast<uint32_t>(SDL_GetTicks())) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    const Uint32 rmask = 0xff000000;
    const Uint32 gmask = 0x00ff0000;
    const Uint32 bmask = 0x0000ff00;
    const Uint32 amask = 0x000000ff;
#else
    const Uint32 rmask = 0x000000ff;
    const Uint32 gmask = 0x0000ff00;
    const Uint32 bmask = 0x00ff0000;
    const Uint32 amask = 0xff000000;
#endif
    pixels = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_RACING_HEIGHT, 32, rmask, gmask, bmask, amask);
    hud = new HUD(StockPaletteAmigaDriving, 4, 24);
    hud->SetVisible(true);
    LoadSpriteBitmaps();
    ResetForNewRace();
}

Racing::Racing(AudioManager* audioMgr) : Racing() {
    audioManager = audioMgr;
}

Racing::~Racing() {
    if (audioManager) {
        audioManager->StopLoop(0);
    }
    if (hud) {
        delete hud;
        hud = nullptr;
    }
    if (pixels) {
        SDL_FreeSurface(pixels);
        pixels = nullptr;
    }
}

void Racing::LoadSpriteBitmaps() {
    needleLines = LoadNeedleTable(state.GetAssets().GetSrExeData());
    for (int slot = 0; slot < 20; ++slot) {
        // Record N is lib.p asset 21 + N.
        const int assetId = RACING_PALM_TREE + slot;
        Bitmap* asset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(assetId)});
        if (asset) {
            roadsideSprites[slot] = std::make_unique<Bitmap>(*asset);
        }
        roadsideAnim[slot] = GetSpriteAnimationInfo(static_cast<uint16_t>(assetId));
    }

    auto* mountainAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_MOUNTAINS});
    if (mountainAsset) {
        mountainSprite = std::make_unique<Bitmap>(*mountainAsset);
    }
    auto* cloudAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CLOUDS});
    if (cloudAsset) {
        cloudSprite = std::make_unique<Bitmap>(*cloudAsset);
    }
    auto* mirrorAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_HORIZON_REARVIEWMIRROR});
    if (mirrorAsset) {
        rearviewMirrorSprite = std::make_unique<Bitmap>(*mirrorAsset);
    }
    auto* countyAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_COUNTY_LINE});
    if (countyAsset) {
        signCountyLineSprite = std::make_unique<Bitmap>(*countyAsset);
    }
    auto* routeAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_ROUTE_69});
    if (routeAsset) {
        signRoute69Sprite = std::make_unique<Bitmap>(*routeAsset);
    }
    auto* exitAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_SIGN_EXIT_48});
    if (exitAsset) {
        signExit48Sprite = std::make_unique<Bitmap>(*exitAsset);
    }
    auto* lightAsset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RED_LIGHTS});
    if (lightAsset) {
        startLightSprite = std::make_unique<Bitmap>(*lightAsset);
    }
}

std::unique_ptr<Bitmap> Racing::LoadOpponentSprite(uint16_t spriteId) {
    byte_vector& libFile = state.GetAssets().GetLibPData();
    BitmapParser bp;
    auto imageEntries = state.GetAssets().GetImageEntries();
    BitmapEntry_t entry = imageEntries.second[spriteId];
    std::vector<uint8_t> data = bp.ReadImage(libFile, entry, spriteId, imageEntries.first);

    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, spriteId);
    // The race colour is the original's one-colour-per-paint table, not a ramp shade. Some sprites carry the magenta
    // placeholder at other palette indices too, so every occurrence is replaced.
    const uint16_t carColor = GetCarRaceColor(opponentColor);
    palData[7] = carColor;
    for (uint16_t& c : palData) {
        if (c == 0x0F0C) {
            c = carColor;
        }
    }

    Palette pal = bp.PreparePalette(palData, true);
    Palette emptyCarPalette;
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, entry, pal, emptyCarPalette, spriteId);
    auto bitmap = std::make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, entry.width, entry.height);
    bitmap->SetDimensions(entry.width, entry.height);
    return bitmap;
}

void Racing::LoadOpponentCarSprites() {
    switch (opponentCarType) {
        case 0:
            opponentChevyFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_FRONT_SIDE);
            opponentChevyRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChevyRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChevyBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY_BEHIND_1 + i);
            }
            break;
        case 1:
            opponentChryslerFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_FRONT_SIDE);
            opponentChryslerRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChryslerRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChryslerBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHRYSLER_BEHIND_1 + i);
            }
            break;
        case 2:
            opponentCopFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_FRONT_SIDE);
            opponentCopRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentCopRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentCopBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_COP_BEHIND_1 + i);
            }
            break;
        case 3:
            opponentOldsmobileFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDSMOBILE_FRONT_SIDE);
            opponentOldsmobileRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentOldsmobileRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentOldsmobileBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_OLDMOBILE_BEHIND_1 + i);
            }
            break;
        case 4:
            opponentChevy2FrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_FRONT_SIDE);
            opponentChevy2RearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentChevy2Rear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentChevy2Behind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CHEVY2_BEHIND_1 + i);
            }
            break;
        case 5:
            opponentCorvette63FrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_FRONT_SIDE);
            opponentCorvette63RearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentCorvette63Rear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentCorvette63Behind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_CORVETTE_BEHIND_1 + i);
            }
            break;
        case 6:
            opponentFordFrontSide = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_FRONT_SIDE);
            opponentFordRearSide = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_REAR_SIDE);
            for (int i = 0; i < 7; ++i) {
                opponentFordRear[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_REAR_1 + i);
            }
            for (int i = 0; i < 4; ++i) {
                opponentFordBehind[i] = LoadOpponentSprite(RACING_OPPONENT_CAR_FORD_BEHIND_1 + i);
            }
            break;
        default:
            opponentCarType = 0;
            LoadOpponentCarSprites();
            break;
    }
}

srrace::CarSetup Racing::BuildCarSetupFromCar(const Car* car, bool isPlayer) const {
    srrace::CarSetup setup;
    if (car) {
        // The car's own gearbox: automaticOverride is set once from the startup car.
        setup = CarSetupFromCar(*car, false);
    } else {
        setup.transClass = automaticOverride ? 0 : 2;
    }
    setup.driverSkill = isPlayer ? 130 : 100;
    return setup;
}

void Racing::InitializeRaceSimulation() {
    // The template belongs to the profile, not the race.
    trackVariant = state.GetTrackVariant();
    srrace::RoadLayoutInit(track, rng);
    srrace::GenerateTrackShapes(track, trackVariant);

    const Car* playerCar = state.GetCurrentCar();
    srrace::CarSetup playerSetup = BuildCarSetupFromCar(playerCar, true);
    // Keep the carless fallback and IsAutomatic()'s pre-init answer in step with the raced car.
    if (playerCar) {
        automaticOverride = (playerSetup.transClass == 0);
    }

    srrace::CarSetup npcSetup;
    Opponent* opponent = state.GetCurrentOpponent();
    if (opponent && opponent->car) {
        npcSetup = BuildCarSetupFromCar(opponent->car.get(), false);
    } else {
        npcSetup = playerSetup;
        npcSetup.driverSkill = 100;
    }
    // The side-swipe roll is rand(aggression/4 + 2).
    if (const SrDriverData* driverData = state.GetCurrentDriverData()) {
        npcSetup.aggression = driverData->aggression;
        // The opponent's upshift point.
        npcSetup.reaction = driverData->reaction;
    }

    // A previous race may have left the cop's sprites in the opponent slot.
    if (opponentCarTypeBeforeCop >= 0) {
        opponentCarType = opponentCarTypeBeforeCop;
        opponentCarTypeBeforeCop = -1;
        LoadOpponentCarSprites();
    }

    // Suppresses the cop spawn and picks the King's engine branches.
    const bool kingRace = state.IsKingOpponent(opponent);
    sim.Init(&track, ToRaceType(currentTrackType), playerSetup, npcSetup,
             static_cast<uint32_t>(SDL_GetTicks()), kingRace);
    simInitialized = true;
    raceBetAmount = state.GetPendingBetAmount();
    raceIsPinkSlip = state.GetPendingIsPinkSlip();
    fixedAccumulator = 0.0f;
    havePrevStep = false;
    prevSpaceDown = false;
    pendingShift = false;
    firstPlayerFinishTick = -1;
    firstNpcFinishTick = -1;
    simTickCounter = 0;
    speedSumMph = 0;
    // StartCruise sets these again afterwards.
    cruiseMode = false;
    cruiseSignSprite.reset();
    cruiseSignAnim = nullptr;
    raceOutcome = RaceOutcome::Ongoing;
    bShowCrashOverlay = false;
    crashHandled = false;
    startLightHeight = 32;  // full; retracts once the car launches
    // The scene outlives a race, so per-race timers are cleared here.
    tireScreechCooldownTicks = 0;
    copSirenCooldownTicks = 0;
    lastVisibleRow = -1;
}

// The ordinary race loop with the race flag cleared, started at segment 10 / 65 / 115. The finish is the drag
// finish at 380, the arch a race starts under.
void Racing::StartCruise(int startSeg, int signAsset) {
    SetTrackType(SR_TRACK_DRAG_RACE);
    // After InitializeRaceSimulation, which clears cruiseMode.
    InitializeRaceSimulation();
    cruiseMode = true;
    if (signAsset > 0) {
        Bitmap* asset = state.GetAssets().GetBitmapAsset(
            {LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(signAsset)});
        if (asset) {
            cruiseSignSprite = std::make_unique<Bitmap>(*asset);
        }
        cruiseSignAnim = GetSpriteAnimationInfo(static_cast<uint16_t>(signAsset));
    }
    sim.BeginCruiseAt(ClampInt(startSeg, 0, kTrackSegments - 1));
    startLightHeight = 0;    // no start light on a cruise
}

// A cruise ends at the next segment with flag bit 0 on either side, i.e. the next building, so each destination
// has its own start segment.
bool Racing::CruiseFinished() const {
    if (!cruiseMode) {
        return false;
    }
    const int seg = ClampInt(static_cast<int>(sim.Player().segPos) + 1, 0, kTrackSegments - 1);
    return ((track.left[seg] | track.right[seg]) & 0x0001) != 0;
}

// -1 left, +1 right, 0 not in range yet. The buildings are at segment 44 (right), 108 (left) and 156 (left).
int Racing::CruiseArrivalSide() const {
    if (!cruiseMode) {
        return 0;
    }
    const int from = static_cast<int>(sim.Player().segPos);
    for (int k = 1; k <= kCruiseArrivalSegs; ++k) {
        const int seg = ClampInt(from + k, 0, kTrackSegments - 1);
        if (track.right[seg] & 0x0001) {
            return +1;
        }
        if (track.left[seg] & 0x0001) {
            return -1;
        }
    }
    return 0;
}

void Racing::SetTrackType(SrTrackType trackType) {
    currentTrackType = trackType;
}

SrTrackType Racing::GetTrackType() const {
    return currentTrackType;
}

void Racing::LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType) {
    (void)binaryData;
    SetTrackType(trackType);
    InitializeRaceSimulation();
}

void Racing::StopEngineSound() {
    if (engineSoundStarted && audioManager) {
        audioManager->StopLoop(0);
    }
    engineSoundStarted = false;
}

void Racing::ResetForNewRace() {
    // Driving and this Racing are built once and Init() never runs again, so per-race state is cleared here.

    // A loop left running from an unfinished race would stack with the new one.
    StopEngineSound();

    // A race always starts in the cockpit.
    flybyMode = false;
    flybyNpcVisible = false;
    flybySeg = srrace::RACE_START_SEG;
    flybyScrollAcc = 0;
    flybyHeading = 0;
    flybyLateral = 2560;
    opponentActiveBeforeFlyby = false;

    InitializeRaceSimulation();
}

// Body asset -> race-view NPC sprite set: 0 chevy, 1 chrysler, 3 oldsmobile, 4 chevy2/corvette-56-61,
// 5 corvette-63, 6 ford; 2 is the cop, never selected by body. From the game's per-car asset table.
static int NpcCarTypeForBodyAsset(int bodyAsset) {
    switch (bodyAsset) {
        // chevy set (selector 1)
        case DODGE_POLARA_1962_BODY:
        case PLYMOUTH_SAVOY_2DR_1962_BODY:
        case FORD_FAIRLANE_VICTORIA_1955_BODY:
        case FORD_FAIRLANE_500_1956_BODY:
        case CHEVROLET_IMPALA_1958_BODY:
        case VALIANT_V200_1961_BODY:
        case VALIANT_V100_2DR_1963_BODY:
            return 0;
        // chrysler set (selector 2)
        case DODGE_CUSTOM_ROYAL_LANCER_1955_BODY:
        case MERCURY_MONTEREY_1954_BODY:
        case MERCURY_CUSTOM_2DR_1956_BODY:
        case CHEVROLET_BELAIR_SPORTS_COUPE_BODY:
            return 1;
        // oldsmobile set (selector 3) - the prewar bodies
        case CHEVROLET_2DR_COUPE_1940_BODY:
        case CHEVROLET_ROADSTER_1940_BODY:
        case CHEVROLET_MASTER_DELUXE_2DR_1938_BODY:
        case FORD_DEUCE_COUPE_1932_BODY:
        case FORD_DELUXE_2DR_1940_BODY:
            return 3;
        // chevy2 set (selector 4)
        case CHEVROLET_CORVETTE_1956_BODY:
        case CHEVROLET_CORVETTE_1961_BODY:
        case CHEVROLET_CORVETTE_1963_BODY:
        case CHEVROLET_STYLELINE_2DR_1949_BODY:
        case OLDSMOBILE_88_2DR_CUSTOM_1949_BODY:
        case OLDSMOBILE_88_CUSTOM_1952_BODY:
        case PONTIAC_SILVER_STREAK_1950_BODY:
        case FORD_THUNDERBIRD_1957_BODY:
            return 4;
        // corvette-63 set (selector 0) - The King only
        case THE_KING_CHEVROLET_CORVETTE_1963_BODY:
            return 5;
        // ford set (selector 5)
        case FORD_SEDAN_1932_BODY:
            return 6;
        default:
            return -1;
    }
}

void Racing::InitializeOpponent() {
    Opponent* opponent = state.GetCurrentOpponent();
    if (!opponent || !opponent->car) {
        opponentActive = false;
        return;
    }

    opponentActive = true;
    opponentColor = opponent->car->GetColor();
    const CarModel& model = opponent->car->GetModel();
    // sprites.body, not index: The King borrows the plain '63 Corvette's row and only overrides the body art.
    // Other car sets go by index and fall through to the vendor guess.
    const int bodyAsset = (model.carSet == kAmigaCarSet) ? model.sprites.body : model.index;
    const int mappedType = NpcCarTypeForBodyAsset(bodyAsset);
    if (mappedType >= 0) {
        opponentCarType = mappedType;
        LoadOpponentCarSprites();
        return;
    }

    // Only a non-Amiga car set reaches here.
    switch (model.vendor) {
        case CarVendorGM:
            opponentCarType = 0;
            break;
        case CarVendorChrysler:
            opponentCarType = 1;
            break;
        case CarVendorFord:
        case CarVendorCustom:
        case CarVendorUnknown:
        default:
            opponentCarType = 0;
            break;
    }
    LoadOpponentCarSprites();
}

float Racing::GetSpeed() const {
    return static_cast<float>(sim.Player().speed);
}

int16_t Racing::GetCurrentGear() const {
    return sim.Player().gear;
}

int16_t Racing::GetSteerDisplay() const {
    return sim.Player().slideVel;
}

// Delegates to the SDL-free srrace::BuildRaceEndReason.
srrace::RaceEndContext Racing::BuildRaceEndContext() const {
    srrace::RaceEndContext ctx;
    ctx.endFlags = sim.Player().endFlags;
    ctx.playerWon = (raceOutcome == RaceOutcome::PlayerWon);
    ctx.playerLost = (raceOutcome == RaceOutcome::PlayerLost);
    ctx.isPinkSlip = raceIsPinkSlip;
    ctx.betAmount = raceBetAmount;
    ctx.isDragRace = (currentTrackType == SR_TRACK_DRAG_RACE);
    ctx.tiresBlown = sim.PlayerTiresBlown();
    ctx.speedingFine = sim.SpeedingFine();
    ctx.bankroll = state.GetBankroll();
    // The car freezes on the failure bit, so this is still the impact speed.
    ctx.crashSpeed = sim.Player().speed;
    ctx.carValue = state.GetCarValue(state.GetCurrentCar());
    ctx.carStrippedValue = state.GetCarStrippedValue(state.GetCurrentCar());
    return ctx;
}

// In the original's time scale, so a race reads the same at any race speed.
int Racing::RaceTimeTenths() const {
    return simTickCounter * 10 / srrace::SIM_HZ_ORIGINAL;
}

int Racing::AverageSpeedMph() const {
    return simTickCounter > 0 ? speedSumMph / simTickCounter : 0;
}

// (player - start) * 100 / (opponent - start), capped at 99 so a loss never reads 100%.
int Racing::DistancePercent() const {
    const int player = static_cast<int>(sim.Player().segPos) - srrace::RACE_START_SEG;
    const int npc = static_cast<int>(sim.Npc().segPos) - srrace::RACE_START_SEG;
    if (npc <= 0) {
        return 0;
    }
    return std::min(99, player * 100 / npc);
}

bool Racing::IsAutomatic() const {
    if (!simInitialized) {
        return automaticOverride;
    }
    return sim.PlayerTables().automatic;
}

int Racing::GetNumGears() const {
    return sim.PlayerTables().numGears;
}

bool Racing::IsAutoDriveEngaged() const {
    return sim.Player().autoDrive;
}

bool Racing::ShouldShowCrashOverlay() const {
    return bShowCrashOverlay;
}

// INFERRED, not verified: the draw is not statically findable. Blood follows the crash tiers at 31, 60 and 91 mph.
// Tuning knob until the real condition is known.
bool Racing::ShouldShowCrashBlood() const {
    return bShowCrashOverlay && sim.PlayerWrecked() && sim.Player().speed >= 60;
}

RaceOutcome Racing::GetRaceOutcome() const {
    return raceOutcome;
}

void Racing::ResetRaceOutcome() {
    raceOutcome = RaceOutcome::Ongoing;
}

void Racing::HandleOutcomeAfterTicks() {
    if (raceOutcome != RaceOutcome::Ongoing || cruiseMode) {
        return;   // a cruise has no outcome; Driving watches CruiseFinished()
    }

    // Any failure ends the race as a loss, but only a wall hit wrecks the car.
    if (sim.PlayerFailed()) {
        if (!crashHandled) {
            crashHandled = true;
            if (sim.PlayerWrecked()) {
                bShowCrashOverlay = true;
                // The bash sample at full length, one low voice.
                if (audioManager) {
                    audioManager->PlaySound(AudioSampleType::CRASH);
                }
            }
            // Driving applies the drivetrain damage with the rest of the outcome.
        }
        raceOutcome = RaceOutcome::PlayerLost;
        return;
    }

    // The sim reports the race over once the car has stopped past the line; the verdict was latched at the line.
    if (sim.RaceOver()) {
        raceOutcome = sim.PlayerWonAtLine() ? RaceOutcome::PlayerWon
                                            : RaceOutcome::PlayerLost;
    }
}

void Racing::Draw(float fElapsedTime, SDL_Event* event) {
    (void)event;
    if (!simInitialized) {
        InitializeRaceSimulation();
    }
    if (!engineSoundStarted && audioManager) {
        audioManager->StartLoop(AudioSampleType::ENGINE_LOOP, 0.8f);
        engineSoundStarted = true;
    }

    sim.SetInvulnerable(debugInvulnerable);

    if (flybyMode) {
        UpdateFlybyMovement(fElapsedTime);
    } else {
        fixedAccumulator += std::max(0.0f, fElapsedTime);
        fixedAccumulator = std::min(fixedAccumulator, StepSeconds() * 2.0f);

        // Consult the debug UI before reading raw key state.
        const Uint8* keyboard = DebugUiWantsKeyboard() ? nullptr : SDL_GetKeyboardState(nullptr);
        uint16_t baseInput = 0;

        // Pad and keyboard drive the same four sim inputs.
        const float padSteer = gamepad.Axis(SDL_CONTROLLER_AXIS_LEFTX);
        const bool padThrottle = gamepad.TriggerDown(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        const bool padBrake = gamepad.TriggerDown(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        const bool padShift = gamepad.Pressed(SDL_CONTROLLER_BUTTON_A) ||
                              gamepad.Pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

        const bool up = (keyboard && keyboard[SDL_SCANCODE_UP]) || padThrottle;
        const bool down = (keyboard && keyboard[SDL_SCANCODE_DOWN]) || padBrake;
        const bool left = (keyboard && keyboard[SDL_SCANCODE_LEFT]) ||
                          padSteer < 0.0f ||
                          gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        const bool right = (keyboard && keyboard[SDL_SCANCODE_RIGHT]) ||
                           padSteer > 0.0f ||
                           gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        const bool space = keyboard && keyboard[SDL_SCANCODE_SPACE];

        if (up) baseInput |= srrace::INPUT_THROTTLE;
        if (down && !up) baseInput |= srrace::INPUT_BRAKE;
        if (left) baseInput |= srrace::INPUT_STEER_LEFT;
        if (right) baseInput |= srrace::INPUT_STEER_RIGHT;
        if ((space && !prevSpaceDown) || padShift) {
            pendingShift = true;
        }
        prevSpaceDown = space;

        // 1.0 unless analog_steering is on and the stick is steering; keyboard and D-pad stay original.
        float steerAmount = 1.0f;
        if (state.GetAssets().GetAnalogSteeringRef()) {
            const bool digitalSteer =
                (keyboard && (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_RIGHT])) ||
                gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
                gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            if (!digitalSteer) {
                steerAmount = std::fabs(padSteer);
            }
        }

        if (cruiseMode) {
            // The original's cutscene winds rpm up, shifts at 4500 and holds top gear. With real physics, hold the throttle
            // to that point and feather it. Driving turns any key into "skip".
            const srrace::CarState& pc = sim.Player();
            const bool topGear = pc.gear >= sim.PlayerTables().numGears;
            baseInput = (topGear && pc.rpm >= kCruiseRpm) ? 0 : srrace::INPUT_THROTTLE;
            pendingShift = !topGear && pc.rpm > kCruiseRpm;

            // Turn towards the building only in the last segment; the original never slows on a cruise.
            const int side = CruiseArrivalSide();
            if (side != 0) {
                baseInput |= (side < 0) ? srrace::INPUT_STEER_LEFT : srrace::INPUT_STEER_RIGHT;
            }
            // A stick resting off centre must not soften the turn-in.
            steerAmount = 1.0f;
        }

        if (fixedAccumulator >= StepSeconds()) {
            uint16_t inputBits = baseInput;
            if (pendingShift) {
                inputBits |= srrace::INPUT_SHIFT;
                pendingShift = false;
            }
            prevPlayer = sim.Player();
            prevNpc = sim.Npc();
            havePrevStep = true;
            sim.Tick(inputBits, steerAmount);
            ++simTickCounter;
            // For the post-race stats panel's average speed.
            speedSumMph += sim.Player().speed;

            // The impact sound as the player is shoved clear.
            if (sim.ConsumeNpcBump() && audioManager) {
                // `random % 40 + 310` per shot.
                const int period = SRL0_CAR_BASH_PERIOD_BASE +
                                   (std::rand() % SRL0_CAR_BASH_PERIOD_SPREAD);
                audioManager->PlaySound(AudioSampleType::CAR_BASH, 1.0f, 3579545 / period);
            }

            // The renderer switches on opponentCarType.
            if (sim.ConsumeCopSpawn()) {
                if (opponentCarTypeBeforeCop < 0) {
                    opponentCarTypeBeforeCop = opponentCarType;
                }
                opponentCarType = 2;
                LoadOpponentCarSprites();
            }

            // Retriggered on a cooldown: our one-shots queue rather than restart a channel.
            if (copSirenCooldownTicks > 0) {
                --copSirenCooldownTicks;
            }
            if (sim.CopSirenOn() && audioManager) {
                if (copSirenCooldownTicks == 0) {
                    audioManager->PlaySound(AudioSampleType::COP_SIREN);
                    copSirenCooldownTicks = SampleTicks(SRL0_COP_SIREN_LENGTH, SRL0_COP_SIREN_RATE);
                }
            } else {
                copSirenCooldownTicks = 0;
            }

            // See PlayerSkidding().
            if (tireScreechCooldownTicks > 0) {
                --tireScreechCooldownTicks;
            }
            if (sim.PlayerSkidding() && audioManager) {
                if (tireScreechCooldownTicks == 0) {
                    audioManager->PlaySound(AudioSampleType::TIRE_SCREECH);
                    tireScreechCooldownTicks = SampleTicks(SRL0_TIRE_SCREECH_LENGTH, SRL0_TIRE_SCREECH_RATE);
                }
            } else {
                tireScreechCooldownTicks = 0;
            }

            if (sim.Phase() != srrace::RacePhase::Countdown && sim.Player().speed > 0) {
                startLightHeight = std::max(0, startLightHeight - 4);
            }

            fixedAccumulator -= StepSeconds();
        }

        HandleOutcomeAfterTicks();

        // Or it keeps running under the race-end bubble.
        if (raceOutcome != RaceOutcome::Ongoing && engineSoundStarted && audioManager) {
            audioManager->StopLoop(0);
            engineSoundStarted = false;
        }
    }

    if (audioManager) {
        audioManager->UpdateEnginePitch(sim.Player().rpm);
    }

    if (SDL_LockSurface(pixels) != 0) {
        return;
    }

    UpdateRenderView();
    RenderSky();
    lastVisibleRow = BuildRowTableAndGetLastVisible();
    RenderRoad(lastVisibleRow);
    RenderMountains(rows[lastVisibleRow].y);
    DrawTownWalls(lastVisibleRow);
    DrawRoadsideAndArchSprites(lastVisibleRow);
    DrawNpcCar(lastVisibleRow);
    DrawStartLight();
    DrawRearviewMirror();
    DrawSideMirror();

    SDL_UnlockSurface(pixels);
    if (flybyMode) {
        DrawFlybyHud(lastVisibleRow);
    } else {
        DrawHud();
    }
}

SDL_Surface* Racing::GetSurface() {
    return pixels;
}
