#include "race/racing.h"
#include "debug/debug_ui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
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

#include <filesystem>

namespace {

constexpr int kForwardRows = 15;
constexpr int kTrackSegments = srrace::TRACK_SEGMENTS;

// The Amiga race loop is not vblank-locked: one physics step per rendered frame, well under 50 fps on an A500.
// Tuning knob: lower = slower race. Rendering stays at GAME_FPS.
constexpr int kPhysicsHz = 15;

// The rpm the drive cutscene shifts at and then holds in top gear.
constexpr int kCruiseRpm = 4500;
// How far ahead the arriving car turns in. One segment shows no turn, twelve drifts across the road; tune here.
constexpr int kCruiseArrivalSegs = 3;
constexpr float kFixedStepSeconds = 1.0f / static_cast<float>(kPhysicsHz);

// This surface lands at screen y = 14, so every screen-space Y from the original is converted.
constexpr int kViewOffsetY = 14;
constexpr int kHorizonY = 60 - kViewOffsetY;    // ProjectForwardY base, screen 60
constexpr int kClipTop = 18 - kViewOffsetY;     // forward viewport clip, screen 18..99
constexpr int kClipBottom = 99 - kViewOffsetY;
constexpr int kRoadCenterLat = 2400;

// Races run at night. The grass is the backdrop's foothill green, so they read as one field.
constexpr SDL_Color kSkyColor = {0, 0, 0, 255};
// Town walls, PAL_DRIVING entries 2, 4 and 10. Cruise only.
constexpr SDL_Color kTownWallGrey = {0x99, 0x99, 0x99, 255};   // PAL_DRIVING[2]
constexpr SDL_Color kTownWallDark = {0x33, 0x33, 0x33, 255};   // PAL_DRIVING[4]
constexpr SDL_Color kTownUpper = {0xEE, 0xEE, 0xEE, 255};      // PAL_DRIVING[10]
constexpr SDL_Color kGrassA = {0, 112, 0, 255};
constexpr SDL_Color kRoadA = {0, 0, 0, 255};
constexpr SDL_Color kDashWhite = {235, 235, 235, 255};
constexpr SDL_Color kDashYellow = {235, 215, 40, 255};
constexpr SDL_Color kCurbGray = {170, 170, 170, 255};
constexpr SDL_Color kArchColor = {180, 32, 32, 255};  // overhead gantry: red

// 20 records of 0x36 bytes {loadHandle:u16, bitmapPtr:u32, maskPtr:u32, frames[11]:{srcX,srcY,w,h} u8 each}.
constexpr size_t kSpriteScaleTableOffset = 0x28024;

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
constexpr size_t kSpriteScaleRecordSize = 0x36;
constexpr int kSpriteScaleFrameCount = 11;

std::array<std::array<std::array<uint8_t, 2>, 11>, 20> LoadSpriteScaleTable(const byte_vector& exe) {
    std::array<std::array<std::array<uint8_t, 2>, 11>, 20> table{};
    const size_t needed = kSpriteScaleTableOffset + 20 * kSpriteScaleRecordSize;
    if (exe.size() < needed) {
        LOG_ERROR("LoadSpriteScaleTable: Street_Rod exe too small (%zu < %zu)", exe.size(), needed);
        return table;
    }
    for (int slot = 0; slot < 20; ++slot) {
        const uint8_t* rec = exe.data() + kSpriteScaleTableOffset + slot * kSpriteScaleRecordSize;
        for (int r = 0; r < kSpriteScaleFrameCount; ++r) {
            const uint8_t* frame = rec + 10 + static_cast<size_t>(r) * 4;  // {srcX, srcY, w, h}
            table[slot][r][0] = frame[2];  // w
            table[slot][r][1] = frame[3];  // h
        }
    }
    return table;
}

int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

// sr grows with distance, 0 = nearest. Frames may be authored either way, so the nearest is the largest by area.
int SelectFrameIndexForDistance(const SpriteAnimationInfo* anim, int sr) {
    const int last = anim->frameCount - 1;
    const int srClamped = ClampInt(sr, 0, last);
    const SpriteRect& f0 = anim->frames[0];
    const SpriteRect& fN = anim->frames[last];
    const bool smallestFirst = (fN.w * fN.h) > (f0.w * f0.h);
    return smallestFirst ? (last - srClamped) : srClamped;
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

void Racing::PutPixel(SDL_Surface* s, int x, int y, SDL_Color c) {
    if (!s || x < 0 || x >= s->w || y < 0 || y >= s->h) {
        return;
    }
    const int offset = y * s->pitch + x * s->format->BytesPerPixel;
    Uint8* pixel = static_cast<Uint8*>(s->pixels) + offset;
    *(pixel + s->format->Rshift / 8) = c.r;
    *(pixel + s->format->Gshift / 8) = c.g;
    *(pixel + s->format->Bshift / 8) = c.b;
    if (s->format->BytesPerPixel > 3) {
        *(pixel + s->format->Ashift / 8) = c.a;
    }
}

// gaugeValue is the dial's own 0..130 input; index = value * 63 / 100.
void Racing::DrawGaugeNeedle(SDL_Renderer* renderer, int originX, int originY, int gaugeValue) {
    if (!renderer || needleLines.empty()) {
        return;
    }
    const int clamped = std::max(0, std::min(130, gaugeValue));
    const NeedleLine& e = needleLines[static_cast<size_t>(clamped) * 63 / 100];
    SDL_SetRenderDrawColor(renderer, 96, 64, 0, 255);
    SDL_RenderDrawLine(renderer, originX + e.x0, originY + e.y0,
                                 originX + e.x1, originY + e.y1);
}

void Racing::DrawNearestNeighborScaled(Bitmap* bitmap, int dstX, int dstY, int dstW, int dstH, bool anchorRight, bool anchorBottom,
                                       int srcX0, int srcY0, int srcW, int srcH, bool flipX) {
    if (!bitmap || dstW <= 0 || dstH <= 0) {
        return;
    }
    SDL_Surface* src = bitmap->GetSurface();
    if (!src || src->w <= 0 || src->h <= 0) {
        return;
    }

    if (srcW <= 0) srcW = src->w;
    if (srcH <= 0) srcH = src->h;
    srcX0 = std::max(0, std::min(srcX0, src->w - 1));
    srcY0 = std::max(0, std::min(srcY0, src->h - 1));
    srcW = std::min(srcW, src->w - srcX0);
    srcH = std::min(srcH, src->h - srcY0);
    if (srcW <= 0 || srcH <= 0) {
        return;
    }

    int drawX = dstX;
    int drawY = dstY;
    if (anchorRight) {
        drawX -= dstW;
    }
    if (anchorBottom) {
        drawY -= dstH;
    }

    for (int y = 0; y < dstH; ++y) {
        const int py = drawY + y;
        if (py < 0 || py >= pixels->h) {
            continue;
        }
        const int srcY = srcY0 + (y * srcH) / dstH;
        for (int x = 0; x < dstW; ++x) {
            const int px = drawX + x;
            if (px < 0 || px >= pixels->w) {
                continue;
            }
            const int col = (x * srcW) / dstW;
            const int srcX = flipX ? (srcX0 + srcW - 1 - col) : (srcX0 + col);
            Uint32 srcPixel = 0;
            const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + srcY * src->pitch + srcX * src->format->BytesPerPixel;
            std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
            if (a == 0) {
                continue;
            }
            PutPixel(pixels, px, py, {r, g, b, a});
        }
    }
}

void Racing::LoadSpriteBitmaps() {
    spriteScale = LoadSpriteScaleTable(state.GetAssets().GetSrExeData());
    needleLines = LoadNeedleTable(state.GetAssets().GetSrExeData());
    for (int slot = 0; slot < 20; ++slot) {
        // Record N is lib.p asset 21 + N.
        const int assetId = RACING_PALM_TREE + slot;
        Bitmap* asset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(assetId)});
        if (asset) {
            roadsideSprites[slot] = std::make_unique<Bitmap>(*asset);
        }
        // Pre-drawn frames when the sprite has them, else the kSpriteScale fallback.
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
    cloudOffsetAccum = 0.0f;
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

void Racing::ResetForNewRace() {
    // Driving and this Racing are built once and Init() never runs again, so per-race state is cleared here.

    // A loop left running from an unfinished race would stack with the new one.
    if (engineSoundStarted && audioManager) {
        audioManager->StopLoop(0);
    }
    engineSoundStarted = false;

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

void Racing::SetOpponentActive(bool active) {
    opponentActive = active;
}

void Racing::SetOpponentCarType(int carType) {
    opponentCarType = carType;
    LoadOpponentCarSprites();
}

void Racing::SetOpponentColor(StandardCarColor color) {
    opponentColor = color;
    LoadOpponentCarSprites();
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

int16_t Racing::GetRPM() const {
    return sim.Player().rpm;
}

int16_t Racing::GetVelocity() const {
    return sim.Player().speed;
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

// The original's `ticks * 10 / 6` assumes 6 steps a second; ours runs at kPhysicsHz.
int Racing::RaceTimeTenths() const {
    return simTickCounter * 10 / kPhysicsHz;
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

bool Racing::IsTransmissionDamaged() const {
    return sim.Player().transWearDelta > 0;
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

int Racing::ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const {
    const int adjustedLat = lateral + (roadDelta * d) / 4;
    const int numerator = (d >> 1) + 4 * (player.lateral - adjustedLat);
    return 160 - numerator / d;
}

int Racing::BuildRowTableAndGetLastVisible() {
    const srrace::CarState& p = sim.Player();
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);
    const int elevAtCar = p.elevAtCar;

    for (int k = 0; k < kForwardRows; ++k) {
        int d = 4;
        if (k > 0) {
            d = 12 + 8 * (k - 1) - p.scrollAcc;
            d = std::max(4, d);
        }
        // The horizontal projection reads curve[], the vertical elev[]. Do not swap them.
        const int seg = ClampInt(segBase + k + 1, 0, kTrackSegments - 1);
        const int roadDelta = p.heading - track.curve[seg];
        const int rowElevDelta = elevAtCar - track.elev[seg];
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);

        RowProjection row;
        row.distance = d;
        row.seg = seg;
        row.roadDelta = roadDelta;
        row.leftX = ProjectX(p, d, roadDelta, edgeLeft);
        row.rightX = ProjectX(p, d, roadDelta, edgeRight);
        row.centerX = ProjectX(p, d, roadDelta, kRoadCenterLat);
        row.shoulderLX = ProjectX(p, d, roadDelta, edgeLeft - 400);
        row.shoulderRX = ProjectX(p, d, roadDelta, edgeRight + 400);
        row.y = ClampInt(kHorizonY + 400 / d + 4 * rowElevDelta, kClipTop, kClipBottom + 30);
        //   y - 4*(y - elevated_y), and elevated_y = y - 400/d, so y - 1600/d.
        row.archY = row.y - 1600 / d;
        rows[k] = row;
    }

    int horizonRow = 0;
    int minY = rows[0].y;
    for (int k = 1; k < kForwardRows; ++k) {
        if (rows[k].y < minY) {
            minY = rows[k].y;
            horizonRow = k;
        }
    }
    return horizonRow;
}

void Racing::RenderSkyAndParallax(float frameSeconds) {
    // The whole surface: near sprites can draw below kClipBottom and would ghost there.
    for (int y = 0; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }

    const srrace::CarState& p = sim.Player();
    cloudOffsetAccum += static_cast<float>(p.heading) * static_cast<float>(p.speed) * frameSeconds * 0.005f;
}

// Drawn between the road's sky fill and the mountains, which would otherwise erase it. Clipped to the horizon.
void Racing::RenderClouds(int horizonY) {
    if (!cloudSprite || !cloudSprite->GetSurface()) {
        return;
    }
    SDL_Surface* src = cloudSprite->GetSurface();
    const int baseOffset = ((static_cast<int>(cloudOffsetAccum) % src->w) + src->w) % src->w;
    const int yEnd = std::min({src->h, horizonY, static_cast<int>(GAME_RACING_HEIGHT)});
    for (int pass = 0; pass < 2; ++pass) {
        const int drawX = -baseOffset + pass * src->w;
        for (int y = 0; y < yEnd; ++y) {
            for (int x = 0; x < src->w; ++x) {
                const int px = drawX + x;
                if (px < 0 || px >= GAME_WIDTH) {
                    continue;
                }
                Uint32 srcPixel = 0;
                const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 255;
                SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
                if (a > 0) {
                    PutPixel(pixels, px, y, {r, g, b, a});
                }
            }
        }
    }
}

// Drawn after the road fill, base on the horizon. Pans with heading, never moves vertically.
void Racing::RenderMountains(int horizonY) {
    if (!mountainSprite || !mountainSprite->GetSurface()) {
        RenderClouds(horizonY);   // no range loaded, but the sky still gets its stars
        return;
    }
    SDL_Surface* src = mountainSprite->GetSurface();
    // A direct function of heading, not an integral of speed, so it returns as the heading returns.
    const int heading = sim.Player().heading;
    const int baseOffset = (((heading * 2) % src->w) + src->w) % src->w;

    // The silhouette/foothill boundary is the horizon; anchoring the lowest opaque row left a green gap.
    if (mountainBaseRow < 0) {
        auto rowKind = [&](int y) {  // 0 = transparent, 1 = all foothill green, 2 = silhouette
            bool opaque = false;
            for (int x = 0; x < src->w; ++x) {
                Uint32 sp = 0;
                const Uint8* p = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&sp, p, src->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, src->format, &r, &g, &b, &a);
                if (a == 0) { continue; }
                opaque = true;
                if (r != kGrassA.r || g != kGrassA.g || b != kGrassA.b) { return 2; }
            }
            return opaque ? 1 : 0;
        };
        mountainBaseRow = src->h - 1;
        for (int y = src->h - 1; y >= 0; --y) {
            const int kind = rowKind(y);
            if (kind == 0 || kind == 1) { continue; }  // padding / foothill band
            mountainBaseRow = y;
            break;
        }
    }

    // The silhouette's base sits just above the horizon; the foothill band below is clipped away.
    const int startY = horizonY - 1 - mountainBaseRow;

    // So nothing shows through the gaps between the peaks.
    const int bandTop = std::max(kClipTop, startY);
    for (int y = bandTop; y < horizonY && y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }

    // After every sky fill and before the range, so the peaks stand in front.
    RenderClouds(horizonY);

    for (int pass = 0; pass < 2; ++pass) {
        const int drawX = -baseOffset + pass * src->w;
        for (int y = 0; y < src->h; ++y) {
            const int py = startY + y;
            if (py < kClipTop || py >= horizonY || py >= GAME_RACING_HEIGHT) {
                continue;
            }
            for (int x = 0; x < src->w; ++x) {
                const int px = drawX + x;
                if (px < 0 || px >= GAME_WIDTH) {
                    continue;
                }
                Uint32 srcPixel = 0;
                const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + y * src->pitch + x * src->format->BytesPerPixel;
                std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
                Uint8 r = 0;
                Uint8 g = 0;
                Uint8 b = 0;
                Uint8 a = 255;
                SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
                if (a > 0) {
                    PutPixel(pixels, px, py, {r, g, b, a});
                }
            }
        }
    }
}

// lib.p asset 97, two 24x32 frames: red during staging, green at launch, then retracts 4/frame as the car rolls.
void Racing::DrawStartLight() {
    if (flybyMode || !startLightSprite || !startLightSprite->GetSurface()) {
        return;
    }
    const srrace::RacePhase phase = sim.Phase();
    if (phase == srrace::RacePhase::Ended || startLightHeight <= 0) {
        return;
    }

    const bool green = (phase != srrace::RacePhase::Countdown);
    if (green && sim.Player().speed > 0) {
        startLightHeight -= 4;
        if (startLightHeight <= 0) {
            startLightHeight = 0;
            return;
        }
    }

    const SpriteRect& frame = RACING_LIGHT_FRAMES[green ? 1 : 0];
    SDL_Surface* src = startLightSprite->GetSurface();
    const int dstX = 130;               // original dest (0x82, 0x12)
    // 18 in the full frame, converted to this surface.
    const int dstY = 18 - kViewOffsetY;
    const int srcYoff = 32 - startLightHeight;  // retract from the bottom (srcY = 32 - h)
    for (int y = 0; y < startLightHeight; ++y) {
        const int sy = frame.y + srcYoff + y;
        const int py = dstY + y;
        if (py < 0 || py >= GAME_RACING_HEIGHT) {
            continue;
        }
        for (int x = 0; x < frame.w; ++x) {
            const int sx = frame.x + x;
            const int px = dstX + x;
            if (px < 0 || px >= GAME_WIDTH) {
                continue;
            }
            Uint32 srcPixel = 0;
            const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + sy * src->pitch + sx * src->format->BytesPerPixel;
            std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
            if (a > 0) {
                PutPixel(pixels, px, py, {r, g, b, a});
            }
        }
    }
}

void Racing::RenderRoad(int lastVisible) {
    auto drawDash = [&](int y, int cx, int drawL, int drawR, uint16_t flags) {
        if ((flags & 0x0008) == 0) {
            return;
        }
        const SDL_Color dashColor = (flags & 0x0010) ? kDashWhite : kDashYellow;
        for (int i = 0; i < 4; ++i) {
            const int px = cx - 1 + i;
            if (px >= drawL && px <= drawR) {
                PutPixel(pixels, px, y, dashColor);
            }
        }
    };

    // The whole surface: the bottom rows show through the side-mirror gap.
    for (int y = kClipTop; y < GAME_RACING_HEIGHT; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kGrassA);
        }
    }

    for (int k = 0; k < lastVisible; ++k) {
        const RowProjection& nearRow = rows[k];
        const RowProjection& farRow = rows[k + 1];
        if (nearRow.y <= farRow.y) {
            continue;
        }

        // Extended to the bottom so the road shows through the side-mirror gap.
        const int yStart = ClampInt(nearRow.y - 1, kClipTop, GAME_RACING_HEIGHT - 1);
        const int yEnd = ClampInt(farRow.y, kClipTop, kClipBottom);
        for (int y = yStart; y >= yEnd; --y) {
            const int dy = std::max(1, nearRow.y - farRow.y);
            const int fromNear = nearRow.y - y;
            const int lx = nearRow.leftX + ((farRow.leftX - nearRow.leftX) * fromNear) / dy;
            const int rx = nearRow.rightX + ((farRow.rightX - nearRow.rightX) * fromNear) / dy;
            const int cx = nearRow.centerX + ((farRow.centerX - nearRow.centerX) * fromNear) / dy;
            const int left = ClampInt(std::min(lx, rx), -200, 520);
            const int right = ClampInt(std::max(lx, rx), -200, 520);
            if (right < 0 || left >= GAME_WIDTH) {
                continue;
            }

            const int seg = (fromNear * 2 >= dy) ? nearRow.seg : farRow.seg;
            const uint16_t flags = track.flags[seg];
            const bool city = (flags & 0x0001) != 0;
            // The road is black everywhere, town included (playtested).
            const SDL_Color roadColor = kRoadA;
            const int drawL = std::max(0, left);
            const int drawR = std::min(GAME_WIDTH - 1, right);
            for (int x = drawL; x <= drawR; ++x) {
                PutPixel(pixels, x, y, roadColor);
            }

            if (city) {
                for (int i = 0; i < 2; ++i) {
                    const int curbL = drawL + i;
                    const int curbR = drawR - i;
                    if (curbL >= 0 && curbL < GAME_WIDTH) {
                        PutPixel(pixels, curbL, y, kCurbGray);
                    }
                    if (curbR >= 0 && curbR < GAME_WIDTH) {
                        PutPixel(pixels, curbR, y, kCurbGray);
                    }
                }
            }

            drawDash(y, cx, drawL, drawR, flags);
        }
    }

    if (lastVisible >= 1) {
        const RowProjection& row0 = rows[0];
        const RowProjection& row1 = rows[1];
        const int yStart = ClampInt(row0.y, kClipTop, GAME_RACING_HEIGHT - 1);
        const int dy = std::max(1, row0.y - row1.y);
        const uint16_t nearFlags = track.flags[row0.seg];
        const SDL_Color nearRoadColor = kRoadA;
        for (int y = yStart; y < GAME_RACING_HEIGHT; ++y) {
            const int extra = y - row0.y;
            const int lx = row0.leftX + ((row0.leftX - row1.leftX) * extra) / dy;
            const int rx = row0.rightX + ((row0.rightX - row1.rightX) * extra) / dy;
            const int cx = row0.centerX + ((row0.centerX - row1.centerX) * extra) / dy;
            const int left = std::max(0, std::min(lx, rx));
            const int right = std::min(GAME_WIDTH - 1, std::max(lx, rx));
            for (int x = left; x <= right; ++x) {
                PutPixel(pixels, x, y, nearRoadColor);
            }
            drawDash(y, cx, left, right, nearFlags);
        }
    }

    // Everything above the road's vanishing row is sky: the range stands on it.
    const int skyEnd = ClampInt(rows[lastVisible].y, kClipTop, kClipBottom);
    for (int y = kClipTop; y < skyEnd; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }
}

void Racing::DrawOverheadArch(const RowProjection& row, uint16_t flags, int sr) {
    const int left = row.leftX;
    const int right = row.rightX;
    if (right - left < 4) {
        return;
    }

    const int yTop = ClampInt(row.archY, 0, GAME_RACING_HEIGHT - 1);
    const int yBottom = ClampInt(row.y, 0, GAME_RACING_HEIGHT - 1);
    if (yBottom <= yTop) {
        return;
    }

    for (int y = yTop; y <= yBottom; ++y) {
        PutPixel(pixels, left, y, kArchColor);
        PutPixel(pixels, left + 1, y, kArchColor);
        PutPixel(pixels, right, y, kArchColor);
        PutPixel(pixels, right - 1, y, kArchColor);
    }

    const int barHeight = ClampInt((yBottom - yTop) / 20, 2, 8);
    for (int y = yTop; y < std::min(GAME_RACING_HEIGHT, yTop + barHeight); ++y) {
        for (int x = std::max(0, left); x <= std::min(GAME_WIDTH - 1, right); ++x) {
            PutPixel(pixels, x, y, kArchColor);
        }
    }

    int bannerWidth = ((right - left) * 3) / 4;
    if (bannerWidth < 8) {
        return;
    }

    Bitmap* banner = nullptr;
    uint16_t bannerId = 0;
    if (flags & 0x8000) {
        banner = signRoute69Sprite.get();
        bannerId = RACING_SIGN_ROUTE_69;
    } else if (flags & 0x4000) {
        banner = signExit48Sprite.get();
        bannerId = RACING_SIGN_EXIT_48;
    } else if (flags & 0x2000 || flags & 0x1000) {
        banner = signCountyLineSprite.get();
        bannerId = RACING_SIGN_COUNTY_LINE;
    }
    if (!banner || !banner->GetSurface()) {
        return;
    }

    const int centerX = (left + right) / 2;

    // Composite sheets of pre-drawn scale frames, stepped through by distance at native size.
    const SpriteAnimationInfo* anim = GetSpriteAnimationInfo(bannerId);
    if (anim && anim->frames && anim->frameCount > 0) {
        const SpriteRect& fr = anim->frames[SelectFrameIndexForDistance(anim, sr)];
        DrawNearestNeighborScaled(banner, centerX - fr.w / 2, yTop + 1, fr.w, fr.h, false, false,
                                  fr.x, fr.y, fr.w, fr.h);
        return;
    }

    SDL_Surface* src = banner->GetSurface();
    const int bannerHeight = std::max(1, bannerWidth * src->h / std::max(1, src->w));
    DrawNearestNeighborScaled(banner, centerX - bannerWidth / 2, yTop + 1, bannerWidth, bannerHeight, false, false);
}

// The town, drawn only during a cruise: per row and side, a wall quad on the shoulder line, topped at the
// elevated row.
void Racing::DrawTownWalls(int lastVisible) {
    if (!cruiseMode) {
        return;
    }

    auto quad = [&](int xA, int topA, int botA, int xB, int topB, int botB, SDL_Color c) {
        const int span = xB - xA;
        const int lo = std::min(xA, xB);
        const int hi = std::max(xA, xB);
        for (int x = std::max(0, lo); x <= std::min(GAME_WIDTH - 1, hi); ++x) {
            const int t = (span == 0) ? 0 : ((x - xA) * 256) / span;
            const int top = topA + ((topB - topA) * t) / 256;
            const int bot = botA + ((botB - botA) * t) / 256;
            for (int y = std::max(kClipTop, top); y <= std::min(kClipBottom, bot); ++y) {
                PutPixel(pixels, x, y, c);
            }
        }
    };

    // Far to near, so nearer blocks overdraw. k stops at 1: row 0 is an extra near row the original lacks.
    for (int k = std::min(lastVisible, kForwardRows - 2); k >= 1; --k) {
        const RowProjection& nearRow = rows[k];
        const RowProjection& farRow = rows[k + 1];
        // The elevated row is the 1900 projection, y - 400/d.
        const int farTop = farRow.y - 400 / std::max(1, farRow.distance);
        const int nearTop = nearRow.y - 400 / std::max(1, nearRow.distance);

        // A cross street every sixteen segments (centre flag bit 2). Its surface is drawn into the gap from the road
        // edge, not the shoulder, or an empty gap reads as a field.
        if (track.flags[nearRow.seg] & 0x0004) {
            for (int side = 0; side < 2; ++side) {
                const int xEdge = side ? nearRow.rightX : nearRow.leftX;
                const int lo = side ? std::max(0, xEdge) : 0;
                const int hi = side ? GAME_WIDTH - 1 : std::min(GAME_WIDTH - 1, xEdge);
                for (int y = std::max(kClipTop, nearTop);
                     y <= std::min(kClipBottom, nearRow.y - 2); ++y) {
                    for (int x = lo; x <= hi; ++x) {
                        PutPixel(pixels, x, y, kTownWallDark);
                    }
                }
                const int kerb = ClampInt(nearRow.y - 1, kClipTop, kClipBottom);
                for (int x = lo; x <= hi; ++x) {
                    PutPixel(pixels, x, kerb, kTownUpper);
                }
            }
        }
        // Either row, or a pair with one foot in the gap draws a wall across it.
        if (((track.flags[farRow.seg] | track.flags[nearRow.seg]) & 0x0004) != 0) {
            continue;
        }

        for (int side = 0; side < 2; ++side) {
            const uint16_t f = side ? track.right[farRow.seg] : track.left[farRow.seg];
            const int xFar = side ? farRow.shoulderRX : farRow.shoulderLX;
            const int xNear = side ? nearRow.shoulderRX : nearRow.shoulderLX;

            quad(xFar, farTop, farRow.y, xNear, nearTop, nearRow.y,
                 (f & 0x0010) ? kTownWallDark : kTownWallGrey);

            // Bit 2 stacks a storey above the kerb wall: full height, or half when bit 3 is clear.
            if (f & 0x0004) {
                const int farH = farRow.y - farTop;
                const int nearH = nearRow.y - nearTop;
                const int farUp = (f & 0x0008) ? farH : farH / 2;
                const int nearUp = (f & 0x0008) ? nearH : nearH / 2;
                quad(xFar, farTop - farUp, farTop, xNear, nearTop - nearUp, nearTop,
                     kTownUpper);

                // Where a block starts, cap it with a flat end face so a cross street reads as a street.
                const uint16_t fNear = side ? track.right[nearRow.seg]
                                            : track.left[nearRow.seg];
                if ((fNear & 0x0004) == 0) {
                    const int xOuter = 2 * xNear - xFar;
                    const int yTop = nearTop - nearUp;
                    quad(xOuter, yTop, nearTop - 1, xNear - 1, yTop, nearTop - 1,
                         kTownWallDark);
                }
            }
        }
    }
}

void Racing::DrawRoadsideAndArchSprites(int lastVisible) {
    auto drawSlot = [&](int slot, int k, int side, bool centerInRoad) {
        if (slot < 0 || slot >= 20 || !roadsideSprites[slot]) {
            return;
        }
        const RowProjection& row = rows[k];
        const int sr = ClampInt(k - 2, 0, 10);

        int w = 0, h = 0;
        int srcX0 = 0, srcY0 = 0, srcW = -1, srcH = -1;
        const SpriteAnimationInfo* anim = roadsideAnim[slot];
        if (anim && anim->frames && anim->frameCount > 0) {
            // The distance's frame at native size.
            const SpriteRect& fr = anim->frames[SelectFrameIndexForDistance(anim, sr)];
            w = fr.w;
            h = fr.h;
            srcX0 = fr.x;
            srcY0 = fr.y;
            srcW = fr.w;
            srcH = fr.h;
        } else {
            // TODO: temporary fallback. Nearest-neighbor scaling is not how the original draws scenery; once every roadside
            // object has pre-drawn frames, delete this branch, spriteScale, and the scaling path of DrawNearestNeighborScaled.
            w = spriteScale[slot][sr][0];
            h = spriteScale[slot][sr][1];
        }
        if (w <= 0 || h <= 0) {
            return;
        }

        Bitmap* sprite = roadsideSprites[slot].get();
        if (centerInRoad) {
            const int x = (row.leftX + row.rightX) / 2 - w / 2;
            DrawNearestNeighborScaled(sprite, x, row.y, w, h, false, true, srcX0, srcY0, srcW, srcH);
        } else if (side < 0) {
            DrawNearestNeighborScaled(sprite, row.leftX - 2, row.y, w, h, true, true, srcX0, srcY0, srcW, srcH);
        } else {
            DrawNearestNeighborScaled(sprite, row.rightX + 2, row.y, w, h, false, true, srcX0, srcY0, srcW, srcH);
        }
    };

    // At the roadside, on its flag's side, not over the middle of the road.
    auto drawCruiseSign = [&](int k, int side) {
        const RowProjection& row = rows[k];
        const int sr = ClampInt(k - 2, 0, 10);
        if (!cruiseSignAnim || !cruiseSignAnim->frames || cruiseSignAnim->frameCount <= 0) {
            return;
        }
        const SpriteRect& fr = cruiseSignAnim->frames[SelectFrameIndexForDistance(cruiseSignAnim, sr)];
        const int x = (side < 0) ? row.leftX - 2 : row.rightX + 2;
        DrawNearestNeighborScaled(cruiseSignSprite.get(), x, row.y, fr.w, fr.h,
                                  side < 0, true, fr.x, fr.y, fr.w, fr.h);
    };

    for (int k = std::min(lastVisible, kForwardRows - 1); k >= 1; --k) {
        const int seg = rows[k].seg;
        if (seg < 0 || seg >= kTrackSegments) {
            continue;
        }
        const uint16_t L = track.left[seg];
        const uint16_t R = track.right[seg];
        const uint16_t F = track.flags[seg];
        const bool city = (F & 0x0001) != 0;

        if ((F & 0x0004) == 0) {
            if (L & 0x0020) drawSlot(0, k, -1, false);
            if (L & 0x0100) drawSlot(4, k, -1, false);
            if (L & 0x0800) drawSlot(3, k, -1, false);
            if (L & 0x1000) drawSlot(6, k, -1, false);
            // On a cruise the destination's own sign; the drive stops here.
            if (L & 0x0001) {
                if (cruiseMode && cruiseSignSprite) {
                    drawCruiseSign(k, -1);
                } else {
                    drawSlot(1, k, -1, city);
                }
            }
            // Rocks and the second tree are countryside only; in town the 0x40 object swaps sprites.
            if (!city && (L & 0x0004)) drawSlot(7, k, -1, false);
            if (!city && (L & 0x0008)) drawSlot(8, k, -1, false);
            if (L & 0x0040) drawSlot(city ? 2 : 10, k, -1, false);
            if (!city && (L & 0x0080)) drawSlot(11, k, -1, false);

            if (!city && (R & 0x0004)) drawSlot(7, k, +1, false);
            if (!city && (R & 0x0008)) drawSlot(8, k, +1, false);
            if (R & 0x0040) drawSlot(city ? 2 : 10, k, +1, false);
            if (!city && (R & 0x0080)) drawSlot(11, k, +1, false);
            if (R & 0x0020) drawSlot(0, k, +1, false);
            if (R & 0x0100) drawSlot(4, k, +1, false);
            if (R & 0x0800) drawSlot(3, k, +1, false);
            if (R & 0x1000) drawSlot(6, k, +1, false);
            if (R & 0x2000) drawSlot(18, k, +1, false);
            if (R & 0x4000) drawSlot(19, k, +1, false);
            if (R & 0x0001) {
                if (cruiseMode && cruiseSignSprite) {
                    drawCruiseSign(k, +1);
                } else {
                    drawSlot(1, k, +1, city);
                }
            }

            if (!city && (F & 0x0002)) drawSlot(9, k, +1, false);
            if (F & 0x0100) drawSlot(5, k, +1, false);
            if (F & 0x0800) drawSlot(15, k, +1, false);
            if (F & 0x0200) drawSlot(16, k, +1, false);
            if (F & 0x0400) drawSlot(17, k, +1, false);
        }

        if (F & 0xf000) {
            DrawOverheadArch(rows[k], F, ClampInt(k - 2, 0, 10));
        }
    }
}

// 13 sprites per car in one asset run: front-side, rear-side, 7 rear LOD frames, 4 mirror frames, indexed by
// view state.
Bitmap* Racing::OpponentSpriteForState(int state) const {
    if (state < 0 || state > 12) {
        return nullptr;
    }
    const std::unique_ptr<Bitmap>* frontSide = &opponentChevyFrontSide;
    const std::unique_ptr<Bitmap>* rearSide = &opponentChevyRearSide;
    const std::unique_ptr<Bitmap>* rear = opponentChevyRear;
    const std::unique_ptr<Bitmap>* behind = opponentChevyBehind;
    switch (opponentCarType) {
        case 1:
            frontSide = &opponentChryslerFrontSide; rearSide = &opponentChryslerRearSide;
            rear = opponentChryslerRear; behind = opponentChryslerBehind; break;
        case 2:
            frontSide = &opponentCopFrontSide; rearSide = &opponentCopRearSide;
            rear = opponentCopRear; behind = opponentCopBehind; break;
        case 3:
            frontSide = &opponentOldsmobileFrontSide; rearSide = &opponentOldsmobileRearSide;
            rear = opponentOldsmobileRear; behind = opponentOldsmobileBehind; break;
        case 4:
            frontSide = &opponentChevy2FrontSide; rearSide = &opponentChevy2RearSide;
            rear = opponentChevy2Rear; behind = opponentChevy2Behind; break;
        case 5:
            frontSide = &opponentCorvette63FrontSide; rearSide = &opponentCorvette63RearSide;
            rear = opponentCorvette63Rear; behind = opponentCorvette63Behind; break;
        case 6:
            frontSide = &opponentFordFrontSide; rearSide = &opponentFordRearSide;
            rear = opponentFordRear; behind = opponentFordBehind; break;
        default: break;
    }
    if (state == 0) return frontSide->get();
    if (state == 1) return rearSide->get();
    if (state < 9) return rear[state - 2].get();
    return behind[state - 9].get();
}

void Racing::DrawNpcCar(int lastVisible) {
    if (flybyMode) {
        if (!flybyNpcVisible) {
            return;
        }
    } else if (!opponentActive) {
        return;
    }

    const srrace::CarState& p = sim.Player();
    const srrace::CarState& np = sim.Npc();
    const srrace::NpcState& ai = sim.NpcAi();

    if (ai.viewHidden) {
        return;   // walked off the end of the range
    }
    const int state = ai.viewState;
    if (state >= 9) {
        return;   // behind us: DrawRearviewMirror has it
    }
    Bitmap* sprite = OpponentSpriteForState(state);
    if (!sprite || !sprite->GetSurface()) {
        return;
    }
    SDL_Surface* src = sprite->GetSurface();

    // From the fine gap, not the row table, which mixed in the player's cycling scroll accumulator.
    const int longDist = (static_cast<int>(np.segPos) - static_cast<int>(p.segPos)) * 8
                       + (np.scrollAcc - p.scrollAcc);
    const int d = std::max(4, longDist + (state == 0 ? 20 : 4));
    const int npcSeg = ClampInt(static_cast<int>(np.segPos) + 1, 0, kTrackSegments - 1);
    const int roadDelta = p.heading - track.curve[npcSeg];
    const int elevDelta = p.elevAtCar - track.elev[npcSeg];
    const int x = ProjectX(p, d, roadDelta, np.lateral);
    int y = kHorizonY + 400 / d + 4 * elevDelta;

    if (state == 0) {
        // Right alongside, off to our left: right edge 64 px left of x, never right of the screen edge, hanging down
        // from the road row.
        DrawNearestNeighborScaled(sprite, std::min(0, x - 64 - src->w), y, src->w, src->h,
                                  false, false);
        return;
    }
    if (state == 1) {
        // Just ahead and well left: right edge on x, baseline 17 px low.
        DrawNearestNeighborScaled(sprite, x, y + 0x11, src->w, src->h, true, true);
        return;
    }
    // Ahead: centred, never above the horizon row. With Hill cover on, clip to the sky above the crest row so a car
    // in a dip is hidden. Flat road is excluded, where the opponent can be beyond the row table.
    const RowProjection& crest = rows[static_cast<size_t>(lastVisible)];
    if (state != 0 && lastVisible < kForwardRows - 1 && npcSeg > crest.seg &&
        ::state.GetAssets().GetHillOcclusionRef()) {   // local `state` is the view state
        const int visible = crest.y - (y - src->h);
        if (visible <= 0) {
            return;
        }
        DrawNearestNeighborScaled(sprite, x - src->w / 2, y - src->h, src->w, visible,
                                  false, false, 0, 0, src->w, visible);
        return;
    }
    y = std::max(crest.y, y);
    if (state == 2) {
        y = std::min(y, kClipBottom);   // the nearest frame stops at the viewport floor
    }
    DrawNearestNeighborScaled(sprite, x - src->w / 2, y, src->w, src->h, false, true);
}

void Racing::DrawRearviewMirror() {
    const int mirrorX = 242;
    const int mirrorY = 9;
    const int mirrorW = 76;
    const int mirrorH = 21;

    // Horizon backdrop, the road behind with centre dashes, roadside LODs, then the NPC, clipped to the glass.
    const int gLeft = mirrorX + 1;
    const int gRight = mirrorX + mirrorW - 2;
    const int gTop = mirrorY + 1;
    const int gBottom = mirrorY + mirrorH - 2;
    const int centerX = mirrorX + mirrorW / 2;   // 280, matches ProjectMirrorX
    constexpr int kMirrorRows = 8;               // BuildMirrorRowTables walks 8
    const srrace::CarState& p = sim.Player();
    const int segBase = ClampInt(static_cast<int>(p.segPos), 0, kTrackSegments - 1);

    // Bottom-left anchored at (dstLeft, dstBottom), never spilling outside the glass.
    auto blitClipped = [&](Bitmap* bmp, const SpriteRect& fr, int dstLeft, int dstBottom) {
        SDL_Surface* s = bmp ? bmp->GetSurface() : nullptr;
        if (!s) return;
        for (int yy = 0; yy < fr.h; ++yy) {
            const int py = dstBottom - (fr.h - 1) + yy;
            if (py < gTop || py > gBottom) continue;
            for (int xx = 0; xx < fr.w; ++xx) {
                const int px = dstLeft + xx;
                if (px < gLeft || px > gRight) continue;
                Uint32 sp = 0;
                const Uint8* ptr = static_cast<const Uint8*>(s->pixels) + (fr.y + yy) * s->pitch + (fr.x + xx) * s->format->BytesPerPixel;
                std::memcpy(&sp, ptr, s->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, s->format, &r, &g, &b, &a);
                if (a > 0) PutPixel(pixels, px, py, {r, g, b, a});
            }
        }
    };

    for (int y = gTop; y <= gBottom; ++y) {
        for (int x = gLeft; x <= gRight; ++x) {
            PutPixel(pixels, x, y, kGrassA);
        }
    }

    int horizonY = gTop + 6;
    if (rearviewMirrorSprite && rearviewMirrorSprite->GetSurface()) {
        SDL_Surface* h = rearviewMirrorSprite->GetSurface();
        const int hh = std::min(h->h, gBottom - gTop);
        for (int yy = 0; yy < hh; ++yy) {
            for (int x = gLeft; x <= gRight; ++x) {
                const int sx = 1 + (x - gLeft);           // skip the 1px border
                if (sx >= h->w) continue;
                Uint32 sp = 0;
                const Uint8* ptr = static_cast<const Uint8*>(h->pixels) + yy * h->pitch + sx * h->format->BytesPerPixel;
                std::memcpy(&sp, ptr, h->format->BytesPerPixel);
                Uint8 r = 0, g = 0, b = 0, a = 255;
                SDL_GetRGBA(sp, h->format, &r, &g, &b, &a);
                if (a > 0) PutPixel(pixels, x, gTop + yy, {r, g, b, a});
            }
        }
        horizonY = gTop + hh - 1;
    }
    // The mirror's horizon is fixed at screen y 36.
    horizonY = ClampInt(36 - kViewOffsetY, gTop, gBottom);

    // The forward projection at quarter scale centred on 280: 8 rows backwards, one segment apart, at depths
    // scrollAcc + 8*(row+1), with the same edges and narrowing test.
    auto projectMirrorX = [&](int d, int roadDelta, int lateral) {
        const int adjustedLat = lateral + (roadDelta * d) / 4;
        const int numerator = (d >> 1) + (p.lateral - adjustedLat);
        return centerX - numerator / d;
    };

    struct MirrorRow {
        int y, left, right, shoulderL, shoulderR, center, seg;
    };
    MirrorRow mrows[kMirrorRows];
    for (int k = 0; k < kMirrorRows; ++k) {     // k 0 = nearest
        const int d = std::max(1, p.scrollAcc + 8 * (k + 1));
        const int seg = ClampInt(segBase - (k + 1), 0, kTrackSegments - 1);
        const int roadDelta = p.heading - track.curve[seg];
        int16_t edgeLeft = 1500;
        int16_t edgeRight = 3300;
        srrace::RoadEdges(track.flags[seg], edgeLeft, edgeRight);
        MirrorRow& row = mrows[k];
        row.seg = seg;
        // 36 + 100/d + row_c at quarter scale.
        row.y = horizonY + 100 / d + (p.elevAtCar - track.elev[seg]);
        row.left = projectMirrorX(d, roadDelta, edgeLeft);
        row.right = projectMirrorX(d, roadDelta, edgeRight);
        row.shoulderL = projectMirrorX(d, roadDelta, edgeLeft - 400);
        row.shoulderR = projectMirrorX(d, roadDelta, edgeRight + 400);
        row.center = projectMirrorX(d, roadDelta, kRoadCenterLat);
    }

    auto paintRow = [&](int y, int left, int right, int center, int seg) {
        if (y < gTop || y > gBottom) return;
        const int rl = std::max(gLeft, left);
        const int rr = std::min(gRight, right);
        for (int x = rl; x <= rr; ++x) {
            PutPixel(pixels, x, y, kRoadA);
        }
        // Alternate rows so the centre line reads as dashes.
        if ((track.flags[seg] & 0x0008) && ((y & 1) == 0) && center >= rl && center <= rr) {
            PutPixel(pixels, center, y, (track.flags[seg] & 0x0010) ? kDashWhite : kDashYellow);
        }
    };
    for (int k = kMirrorRows - 1; k >= 0; --k) {
        const MirrorRow& f = mrows[k];
        const int yEnd = (k == 0) ? gBottom : mrows[k - 1].y - 1;
        const int span = std::max(1, yEnd - f.y + 1);
        for (int y = f.y; y <= yEnd; ++y) {
            int l = f.left;
            int r = f.right;
            int c = f.center;
            if (k > 0) {
                const MirrorRow& n = mrows[k - 1];
                const int t = ((y - f.y) * 256) / span;
                l += ((n.left - f.left) * t) / 256;
                r += ((n.right - f.right) * t) / 256;
                c += ((n.center - f.center) * t) / 256;
            }
            paintRow(y, l, r, c, f.seg);
        }
    }

    auto slotForFlags = [](uint16_t f) -> int {
        if (f & 0x0001) return 1;   // building / main
        if (f & 0x0020) return 0;   // palm
        if (f & 0x0100) return 4;
        if (f & 0x0800) return 3;
        if (f & 0x0040) return 10;
        if (f & 0x0004) return 7;
        if (f & 0x0008) return 8;
        return -1;
    };
    for (int k = 1; k < kMirrorRows; ++k) {
        const MirrorRow& row = mrows[k];
        if (row.y < horizonY || row.y > gBottom) continue;
        for (int side = 0; side < 2; ++side) {
            const uint16_t f = side ? track.right[row.seg] : track.left[row.seg];
            const int slot = slotForFlags(f);
            if (slot < 0 || !roadsideSprites[slot]) continue;
            const SpriteAnimationInfo* anim = roadsideAnim[slot];
            if (!anim || !anim->frames || anim->frameCount <= 0) continue;
            const SpriteRect& fr = anim->frames[anim->frameCount - 1];   // farthest/smallest
            const int dstLeft = side ? row.shoulderR : (row.shoulderL - fr.w);
            blitClipped(roadsideSprites[slot].get(), fr, dstLeft, row.y);
        }
    }

    // Quarter scale: elevated y is row.y - 400/d, crossbar four times that above the road. No banner art at 74 px.
    auto putGlass = [&](int x, int y) {
        if (x >= gLeft && x <= gRight && y >= gTop && y <= gBottom) {
            PutPixel(pixels, x, y, kArchColor);
        }
    };
    for (int k = 0; k < kMirrorRows; ++k) {
        const MirrorRow& row = mrows[k];
        if ((track.flags[row.seg] & 0xf000) == 0 || row.right - row.left < 4) {
            continue;
        }
        const int d = std::max(1, p.scrollAcc + 8 * (k + 1));
        const int yBottom = std::min(row.y, gBottom);
        const int yTop = std::max(gTop, row.y - 400 / d);
        if (yBottom <= yTop) {
            continue;
        }
        for (int y = yTop; y <= yBottom; ++y) {
            putGlass(row.left, y);
            putGlass(row.left + 1, y);
            putGlass(row.right, y);
            putGlass(row.right - 1, y);
        }
        const int barHeight = ClampInt((yBottom - yTop) / 6, 1, 3);
        for (int y = yTop; y < yTop + barHeight; ++y) {
            for (int x = row.left; x <= row.right; ++x) {
                putGlass(x, y);
            }
        }
    }

    if (opponentActive) {
        const int delta = static_cast<int>(sim.Npc().segPos - sim.Player().segPos);
        if (delta < 0 && delta >= -8) {
            const int idx = ClampInt((-delta) / 2, 0, 3);
            Bitmap* behind = nullptr;
            switch (opponentCarType) {
                case 1: behind = opponentChryslerBehind[idx].get(); break;
                case 2: behind = opponentCopBehind[idx].get(); break;
                case 3: behind = opponentOldsmobileBehind[idx].get(); break;
                case 4: behind = opponentChevy2Behind[idx].get(); break;
                case 5: behind = opponentCorvette63Behind[idx].get(); break;
                case 6: behind = opponentFordBehind[idx].get(); break;
                default: behind = opponentChevyBehind[idx].get(); break;
            }
            if (behind && behind->GetSurface()) {
                SDL_Surface* bs = behind->GetSurface();
                // Native LOD frame, cropped to fit the glass.
                const int w = std::min(bs->w, mirrorW - 2);
                const int h = std::min(bs->h, mirrorH - 2);
                const int cx = mirrorX + mirrorW / 2 - w / 2;
                const int cy = mirrorY + mirrorH - h - 1;
                DrawNearestNeighborScaled(behind, cx, cy, w, h, false, false, 0, 0, w, h);
            }
        }
    }
}

// The door mirror is the roof mirror's left-hand corner, re-blitted every frame; Driving punches the interior's
// glass so it shows through.
void Racing::DrawSideMirror() {
    if (!pixels || pixels->format->BytesPerPixel != 4) {
        return;
    }
    Uint32* px = static_cast<Uint32*>(pixels->pixels);
    const int stride = pixels->pitch / 4;
    for (int row = 0; row < kSideMirrorRows; ++row) {
        const SideMirrorRow& g = kSideMirrorGlass[row];
        const int w = g.right - g.left + 1;
        const int srcY = kSideMirrorSrcY - kViewOffsetY + row;
        const int dstY = kSideMirrorTop - kViewOffsetY + row;
        if (kSideMirrorSrcX + w > GAME_WIDTH || g.right >= GAME_WIDTH ||
            srcY < 0 || dstY + 1 > GAME_RACING_HEIGHT) {
            continue;
        }
        std::memcpy(&px[dstY * stride + g.left],
                    &px[srcY * stride + kSideMirrorSrcX], w * sizeof(Uint32));
    }
}

void Racing::DrawHud() {
    if (!hud) {
        return;
    }
    if (cruiseMode) {
        // A cutscene has no gear readout.
        return;
    }
    char gearText[16];
    if (IsAutomatic()) {
        std::snprintf(gearText, sizeof(gearText), "Gear: %s",
                      sim.Player().gear == 0 ? "N" : "D");
    } else if (sim.Player().gear == 0) {
        std::snprintf(gearText, sizeof(gearText), "Gear: N");
    } else {
        std::snprintf(gearText, sizeof(gearText), "Gear: %d", sim.Player().gear);
    }
    hud->Clear();
    hud->AddText(6, 0, gearText);
    if (sim.InCountdown()) {
        hud->AddText(104, 45, "STAGE - WAIT FOR GREEN");
    }
    hud->Draw();
}

void Racing::SetDebugFeaturesEnabled(bool enabled) {
    debugFeaturesEnabled = enabled;
    if (!enabled && flybyMode) {
        flybyMode = false;
        opponentActive = opponentActiveBeforeFlyby;
    }
}

std::string Racing::FormatSegmentFlags(uint16_t flags) {
    std::string out;
    if (flags & 0x0001) out += "city ";
    if (flags & 0x0002) out += "sign ";
    if (flags & 0x0004) out += "side ";
    if (flags & 0x0008) out += "dash ";
    if (flags & 0x0010) out += "dashW ";
    if ((flags & 0x00e0) == 0x0020) out += "narrowL ";
    else if ((flags & 0x00e0) == 0x0040) out += "narrowR ";
    else if ((flags & 0x00e0) == 0x0080) out += "narrowB ";
    if (flags & 0x0100) out += "spr5 ";
    if (flags & 0x0200) out += "curveL ";
    if (flags & 0x0400) out += "curveR ";
    if (flags & 0x0800) out += "narrowWarn ";
    if (flags & 0xf000) out += "arch ";
    if (out.empty()) {
        out = "straight";
    }
    return out;
}

void Racing::ApplyFlybyCamera() {
    const int seg = ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1);
    const int16_t elevAtCar = track.elev[seg];
    sim.SetCameraState(flybySeg, flybyScrollAcc, flybyHeading, flybyLateral, elevAtCar);
    if (flybyNpcVisible) {
        sim.SetNpcDebugPose(flybySeg + 6, 2560);
    } else {
        sim.SetNpcDebugPose(flybySeg - 20, 2560);
    }
}

void Racing::FlybyScroll(int scrollDelta) {
    if (scrollDelta == 0) {
        return;
    }
    int32_t seg = flybySeg;
    int acc = static_cast<int>(flybyScrollAcc) + scrollDelta;
    while (acc > 7) {
        seg += 1;
        acc -= 8;
    }
    while (acc < 0) {
        seg -= 1;
        acc += 8;
    }
    constexpr int kMaxSeg = srrace::TRACK_SEGMENTS - 1;
    if (seg < 0) {
        seg = 0;
        acc = 0;
    } else if (seg > kMaxSeg) {
        seg = kMaxSeg;
        acc = 7;
    }
    flybySeg = seg;
    flybyScrollAcc = static_cast<int16_t>(acc);
    ApplyFlybyCamera();
}

void Racing::FlybyJumpSegments(int segDelta) {
    if (segDelta == 0) {
        return;
    }
    int32_t seg = flybySeg + segDelta;
    seg = std::max<int32_t>(0, std::min<int32_t>(seg, srrace::TRACK_SEGMENTS - 1));
    flybySeg = seg;
    ApplyFlybyCamera();
}

void Racing::UpdateFlybyMovement(float frameSeconds) {
    // Key state is polled from SDL, so the debug UI's keyboard capture is checked explicitly.
    const Uint8* keyboard = DebugUiWantsKeyboard() ? nullptr : SDL_GetKeyboardState(nullptr);
    if (!keyboard) {
        return;
    }

    static float scrollRepeat = 0.0f;
    const bool forward = keyboard[SDL_SCANCODE_RIGHTBRACKET] != 0;
    const bool backward = keyboard[SDL_SCANCODE_LEFTBRACKET] != 0;
    if (!forward && !backward) {
        scrollRepeat = 0.0f;
        return;
    }

    scrollRepeat += frameSeconds;
    const float interval = (keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT]) ? 0.03f : 0.06f;
    if (scrollRepeat < interval) {
        return;
    }
    scrollRepeat = 0.0f;

    const bool shift = keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT];
    const int step = shift ? 8 : 1;
    FlybyScroll(forward ? step : -step);
}

bool Racing::ToggleFlybyMode() {
    if (!debugFeaturesEnabled) {
        return false;
    }
    if (!simInitialized) {
        InitializeRaceSimulation();
    }

    if (!flybyMode) {
        const srrace::CarState& p = sim.Player();
        flybySeg = p.segPos;
        flybyScrollAcc = p.scrollAcc;
        flybyHeading = p.heading;
        flybyLateral = p.lateral;
        opponentActiveBeforeFlyby = opponentActive;
        opponentActive = false;
        flybyNpcVisible = false;
        raceOutcome = RaceOutcome::Ongoing;
        bShowCrashOverlay = false;
    } else {
        opponentActive = opponentActiveBeforeFlyby;
    }

    flybyMode = !flybyMode;
    if (flybyMode) {
        ApplyFlybyCamera();
    }
    return true;
}

bool Racing::OnFlybyKeyDown(SDL_Keycode key) {
    if (!flybyMode) {
        return false;
    }

    switch (key) {
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            flybyHeading = static_cast<int16_t>(flybyHeading + 1);
            ApplyFlybyCamera();
            return true;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            flybyHeading = static_cast<int16_t>(flybyHeading - 1);
            ApplyFlybyCamera();
            return true;
        case SDLK_COMMA:
            flybyLateral = static_cast<int16_t>(flybyLateral - 40);
            ApplyFlybyCamera();
            return true;
        case SDLK_PERIOD:
            flybyLateral = static_cast<int16_t>(flybyLateral + 40);
            ApplyFlybyCamera();
            return true;
        case SDLK_LEFTBRACKET: {
            const Uint8* kb = SDL_GetKeyboardState(nullptr);
            const bool ctrl = kb && (kb[SDL_SCANCODE_LCTRL] || kb[SDL_SCANCODE_RCTRL]);
            if (ctrl) {
                FlybyJumpSegments(-10);
                return true;
            }
            return false;
        }
        case SDLK_RIGHTBRACKET: {
            const Uint8* kb = SDL_GetKeyboardState(nullptr);
            const bool ctrl = kb && (kb[SDL_SCANCODE_LCTRL] || kb[SDL_SCANCODE_RCTRL]);
            if (ctrl) {
                FlybyJumpSegments(10);
                return true;
            }
            return false;
        }
        case SDLK_HOME:
            flybySeg = srrace::RACE_START_SEG;
            flybyScrollAcc = 0;
            ApplyFlybyCamera();
            return true;
        case SDLK_END:
            flybySeg = sim.FinishSegment() - 1;
            flybyScrollAcc = 0;
            ApplyFlybyCamera();
            return true;
        case SDLK_PAGEUP:
            FlybyJumpSegments(50);
            return true;
        case SDLK_PAGEDOWN:
            FlybyJumpSegments(-50);
            return true;
        case SDLK_n:
            flybyNpcVisible = !flybyNpcVisible;
            ApplyFlybyCamera();
            return true;
        default:
            break;
    }
    return false;
}

std::vector<std::string> Racing::GetFlybyDebugLines(int lastVisibleRow) const {
    std::vector<std::string> lines;

    const srrace::CarState& player = sim.Player();
    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(player.segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc)
                              : static_cast<int>(player.scrollAcc);
    const int heading = flybyMode ? static_cast<int>(flybyHeading)
                                  : static_cast<int>(player.heading);
    const int lateral = flybyMode ? static_cast<int>(flybyLateral)
                                  : static_cast<int>(player.lateral);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s seg %d +%d/8  var %d  %s",
                  flybyMode ? "FLYBY" : "race", seg, acc, trackVariant,
                  currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road");
    lines.push_back(buf);

    std::snprintf(buf, sizeof(buf), "curve %d elev %d  heading %d  lat %d",
                  static_cast<int>(track.curve[seg]),
                  static_cast<int>(track.elev[seg]),
                  heading, lateral);
    lines.push_back(buf);

    if (!flybyMode) {
        std::snprintf(buf, sizeof(buf), "speed %d  rpm %d  gear %d  step %d",
                      static_cast<int>(player.speed), static_cast<int>(player.rpm),
                      static_cast<int>(player.gear), static_cast<int>(player.scrollStep));
        lines.push_back(buf);
    }

    std::snprintf(buf, sizeof(buf), "flags 0x%04x %s  Lobj 0x%04x Robj 0x%04x",
                  static_cast<unsigned>(track.flags[seg]),
                  FormatSegmentFlags(track.flags[seg]).c_str(),
                  static_cast<unsigned>(track.left[seg]),
                  static_cast<unsigned>(track.right[seg]));
    lines.push_back(buf);

    if (lastVisibleRow >= 0 && lastVisibleRow < static_cast<int>(rows.size())) {
        const RowProjection& row = rows[static_cast<size_t>(lastVisibleRow)];
        std::snprintf(buf, sizeof(buf), "row0 seg %d y %d L %d R %d d %d",
                      row.seg, row.y, row.leftX, row.rightX, row.distance);
        lines.push_back(buf);
    }

    if (flybyMode) {
        lines.push_back("[ ] step  shift+[ ] seg  ctrl+[ ] +/-10seg");
        lines.push_back("=/- heading  ,/. lateral  Home/End start/finish");
        lines.push_back("PgUp/Dn +/-50seg  N toggle NPC  F9 exit flyby");
        lines.push_back("P save road PNG (no cockpit)");
    } else {
        lines.push_back("F9 enter flyby");
    }
    return lines;
}

std::vector<DebugRow> Racing::GetRacingDebugRows(int lastVisibleRow) const {
    std::vector<DebugRow> out;

    const srrace::CarState& player = sim.Player();
    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(player.segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc)
                              : static_cast<int>(player.scrollAcc);
    const int heading = flybyMode ? static_cast<int>(flybyHeading)
                                  : static_cast<int>(player.heading);
    const int lateral = flybyMode ? static_cast<int>(flybyLateral)
                                  : static_cast<int>(player.lateral);

    char buf[160];
    out.push_back({"track", ""});
    out.push_back({"mode", flybyMode ? "flyby camera" : "racing"});
    out.push_back({"track", currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road"});
    out.push_back({"variant", std::to_string(trackVariant)});

    std::snprintf(buf, sizeof(buf), "%d  +%d/8", seg, acc);
    out.push_back({"segment", buf});
    out.push_back({"curve", std::to_string(static_cast<int>(track.curve[seg]))});
    out.push_back({"elevation", std::to_string(static_cast<int>(track.elev[seg]))});
    out.push_back({"heading", std::to_string(heading)});
    out.push_back({"lateral", std::to_string(lateral)});

    std::snprintf(buf, sizeof(buf), "0x%04x %s", static_cast<unsigned>(track.flags[seg]),
                  FormatSegmentFlags(track.flags[seg]).c_str());
    out.push_back({"flags", buf});
    std::snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(track.left[seg]));
    out.push_back({"left object", buf});
    std::snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(track.right[seg]));
    out.push_back({"right object", buf});

    out.push_back({"player car", ""});
    out.push_back({"speed", std::to_string(static_cast<int>(player.speed)) + " mph"});
    out.push_back({"rpm", std::to_string(static_cast<int>(player.rpm))});
    out.push_back({"gear", std::to_string(static_cast<int>(player.gear))});
    out.push_back({"scroll step", std::to_string(static_cast<int>(player.scrollStep))});
    out.push_back({"lap distance", std::to_string(player.distanceAcc)});

    if (lastVisibleRow >= 0 && lastVisibleRow < static_cast<int>(rows.size())) {
        const RowProjection& row = rows[static_cast<size_t>(lastVisibleRow)];
        std::snprintf(buf, sizeof(buf), "seg %d  y %d  L %d  R %d  d %d",
                      row.seg, row.y, row.leftX, row.rightX, row.distance);
        out.push_back({"far row", buf});
    } else {
        out.push_back({"far row", "-"});
    }

    // Segments scaled by 8 plus the scroll remainder; positive means ahead.
    const srrace::CarState& npc = sim.Npc();
    const int segGap = static_cast<int>(npc.segPos) - static_cast<int>(player.segPos);
    const int fineGap = segGap * 8 + (npc.scrollAcc - player.scrollAcc);
    out.push_back({"opponent", ""});
    out.push_back({"active", opponentActive ? "yes" : "no"});
    static const char* const kCarTypeNames[] = {
        "chevy", "chrysler", "cop", "oldsmobile", "chevy2 (corvette 56/61)", "corvette 63", "ford"
    };
    const char* carTypeName = (opponentCarType >= 0 &&
                                opponentCarType < static_cast<int>(std::size(kCarTypeNames)))
                                   ? kCarTypeNames[opponentCarType]
                                   : "chevy";
    out.push_back({"car type", carTypeName});
    std::snprintf(buf, sizeof(buf), "%d seg  (%d units)", segGap, fineGap);
    out.push_back({"gap", buf});
    out.push_back({"segment", std::to_string(static_cast<int>(npc.segPos))});
    out.push_back({"speed", std::to_string(static_cast<int>(npc.speed)) + " mph"});
    out.push_back({"lateral", std::to_string(static_cast<int>(npc.lateral))});
    out.push_back({"lateral gap", std::to_string(npc.lateral - player.lateral)});
    out.push_back({"view state", std::to_string(static_cast<int>(sim.NpcAi().viewState)) +
                                 (sim.NpcAi().viewHidden ? " (hidden)" : "")});

    // State 0: the spawn roll did not arm one.
    const int copState = sim.CopState();
    out.push_back({"cop", ""});
    // Always three rows, so nothing below shifts when the state changes.
    const bool copPossible = sim.RaceType() != 0 && copState != 0;
    out.push_back({"can appear", sim.RaceType() == 0 ? "no (drag race)"
                                 : copState == 0     ? "no (roll failed)"
                                                     : "yes"});
    out.push_back({"trigger seg",
                   copPossible ? std::to_string(sim.CopTriggerSegment()) : std::string("-")});
    if (copPossible) {
        const int toTrigger = sim.CopTriggerSegment() - static_cast<int>(player.segPos);
        out.push_back({"to trigger", toTrigger > 0 ? std::to_string(toTrigger) + " seg"
                                                   : std::string("passed")});
    } else {
        out.push_back({"to trigger", "-"});
    }
    const char* copStateName = copState == 0   ? "not armed"
                               : copState == 1 ? "armed, waiting"
                               : copState == 2 ? "pursuing"
                                               : "pursuing, siren on";
    out.push_back({"state", copStateName});
    out.push_back({"busted at", sim.PlayerBusted()
                                    ? std::to_string(sim.CopCaughtSpeed()) + " mph"
                                    : std::string("-")});
    out.push_back({"fine", sim.PlayerBusted() ? "$" + std::to_string(sim.SpeedingFine())
                                              : std::string("-")});
    return out;
}

std::vector<DebugRow> Racing::GetOpponentDebugRows() const {
    std::vector<DebugRow> out;
    const srrace::CarState& player = sim.Player();
    const srrace::CarState& npc = sim.Npc();
    const srrace::NpcState& ai = sim.NpcAi();

    const char* branch = "accelerating";
    switch (ai.branch) {
        case srrace::NpcBranch::HoldingLevel:  branch = "holding level (player +16)"; break;
        case srrace::NpcBranch::CruisingAhead: branch = "clear ahead, cruising"; break;
        case srrace::NpcBranch::CopClosingCap: branch = "cop cap (player +32)"; break;
        case srrace::NpcBranch::CorneringHold: branch = "in a curve, holding"; break;
        case srrace::NpcBranch::CorneringCap:  branch = "braked for the curve"; break;
        case srrace::NpcBranch::Accelerating:  break;
    }

    out.push_back({"driving", ""});
    out.push_back({"decision", branch});
    out.push_back({"eased to", ai.branchTarget > 0
                                   ? std::to_string(ai.branchTarget) + " mph"
                                   : std::string("-")});
    out.push_back({"gave up", ai.gaveUp ? "yes" : "no"});
    out.push_back({"stalled", ai.stallTicks > 0
                                  ? std::to_string(ai.stallTicks) + " ticks"
                                  : std::string("-")});

    out.push_back({"steering", ""});
    out.push_back({"lane target", std::to_string(ai.lateralTarget)});
    out.push_back({"lateral", std::to_string(static_cast<int>(npc.lateral))});

    out.push_back({"car", ""});
    out.push_back({"speed", std::to_string(static_cast<int>(npc.speed)) + " mph"});
    out.push_back({"rpm", std::to_string(static_cast<int>(npc.rpm))});
    out.push_back({"gear", std::to_string(static_cast<int>(npc.gear))});
    out.push_back({"cruise / risky", std::to_string(ai.targetSpeedNormal) + " / " +
                                     std::to_string(ai.targetSpeedRisky)});
    out.push_back({"performance", std::to_string(sim.NpcTables().perfScore)});
    out.push_back({"top speed", std::to_string(sim.NpcTables().topSpeed) + " mph"});

    const int segGap = static_cast<int>(npc.segPos) - static_cast<int>(player.segPos);
    out.push_back({"gap", std::to_string(segGap) + " seg"});
    return out;
}

std::vector<DebugIndicator> Racing::GetOpponentIndicators() const {
    const srrace::CarState& npc = sim.Npc();
    const srrace::NpcState& ai = sim.NpcAi();

    // The steering's own deadband, so the lamp doesn't flicker.
    const int latDelta = ai.lateralTarget - npc.lateral;
    const bool left = latDelta < -8;
    const bool right = latDelta > 8;

    return {
        {"<<", left},
        {"--", !left && !right},
        {">>", right},
        {"GAS", ai.throttle},
        {"SHIFT", ai.shifted},
        {"EASED", ai.branch != srrace::NpcBranch::Accelerating &&
                  ai.branch != srrace::NpcBranch::CorneringHold},
    };
}

bool Racing::SaveRoadViewPng(std::string* savedPath) {
    if (!pixels) {
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path dir = "roadfly";
    std::error_code ec;
    fs::create_directories(dir, ec);

    const int seg = flybyMode
                        ? ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1)
                        : ClampInt(static_cast<int>(sim.Player().segPos), 0, srrace::TRACK_SEGMENTS - 1);
    const int acc = flybyMode ? static_cast<int>(flybyScrollAcc) : static_cast<int>(sim.Player().scrollAcc);
    const char* trackTag = currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road";

    std::string path;
    for (int n = 0; n < 10000; ++n) {
        char buf[192];
        if (n == 0) {
            std::snprintf(buf, sizeof(buf), "roadfly/%s_seg%03d_acc%d_var%d.png",
                          trackTag, seg, acc, trackVariant);
        } else {
            std::snprintf(buf, sizeof(buf), "roadfly/%s_seg%03d_acc%d_var%d_%d.png",
                          trackTag, seg, acc, trackVariant, n);
        }
        path = buf;
        if (!fs::exists(path)) {
            break;
        }
    }

    SDL_Surface* src = pixels;
    sdl2::SurfacePtr converted;
    if (src->format->format != SDL_PIXELFORMAT_RGBA32) {
        converted = sdl2::SurfacePtr(SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0));
        if (!converted) {
            LOG_INFO("SaveRoadViewPng: surface convert failed: %s", SDL_GetError());
            return false;
        }
        src = converted.get();
    }

    if (IMG_SavePNG(src, path.c_str()) != 0) {
        LOG_INFO("SaveRoadViewPng: save failed: %s", SDL_GetError());
        return false;
    }

    LOG_INFO("Road view saved: %s", path.c_str());
    if (savedPath != nullptr) {
        *savedPath = path;
    }
    return true;
}

void Racing::DrawFlybyHud(int lastVisible) {
    if (!hud) {
        return;
    }
    hud->Clear();
    hud->AddText(6, 0, "FLYBY");
    const std::vector<std::string> lines = GetFlybyDebugLines(lastVisible);
    int y = 10;
    for (size_t i = 1; i < lines.size() && i < 4; ++i) {
        hud->AddText(6, y, lines[i].c_str());
        y += 10;
    }
    hud->Draw();
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
        fixedAccumulator = std::min(fixedAccumulator, kFixedStepSeconds * 2.0f);

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

        if (fixedAccumulator >= kFixedStepSeconds) {
            uint16_t inputBits = baseInput;
            if (pendingShift) {
                inputBits |= srrace::INPUT_SHIFT;
                pendingShift = false;
            }
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
                    copSirenCooldownTicks = 13;
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
                    // ~445 ms sample at 15 Hz.
                    tireScreechCooldownTicks = 7;
                }
            } else {
                tireScreechCooldownTicks = 0;
            }

            fixedAccumulator -= kFixedStepSeconds;
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

    RenderSkyAndParallax(fElapsedTime);
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

void Racing::DrawTachometerNeedleToRenderer(SDL_Renderer* renderer) {
    // The dial takes rpm / 43 and clamps at 130, so full scale is 5590: the needle pegs just before the 5700 redline,
    // the over-rev warning.
    DrawGaugeNeedle(renderer, 145, 129, sim.Player().rpm / 43);
}

void Racing::DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer) {
    // The dial clamps at 130, so full scale is 130 mph.
    DrawGaugeNeedle(renderer, 196, 129, sim.Player().speed);
}

SDL_Surface* Racing::GetSurface() {
    return pixels;
}
