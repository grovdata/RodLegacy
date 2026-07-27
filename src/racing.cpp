#include "racing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>

#include "car.h"
#include "car_system.h"
#include "engine_part.h"
#include "global.h"
#include "log.h"
#include "part.h"
#include "race/track_gen.h"
#include "state.h"

#include <filesystem>

namespace {

constexpr int kForwardRows = 15;
constexpr int kTrackSegments = srrace::TRACK_SEGMENTS;

// Physics tick rate (Hz). The Amiga race loop (RoadRaceMainLoop 0x2360c2) is NOT
// vblank-locked: it runs one physics step per rendered frame, and on a stock
// A500 that is well under 50 fps, so a true 50 Hz sim feels ~2-2.5x too fast.
// TUNE THIS: lower = slower race, higher = faster. Rendering stays at GAME_FPS
// and just samples the sim, so this only changes physics speed/feel.
constexpr int kPhysicsHz = 15;
constexpr float kFixedStepSeconds = 1.0f / static_cast<float>(kPhysicsHz);

constexpr int kHorizonY = 60;
constexpr int kClipTop = 18;
constexpr int kClipBottom = 99;
constexpr int kRoadCenterLat = 2400;

// Races run at night in the original game.
constexpr SDL_Color kSkyColor = {0, 0, 0, 255};
constexpr SDL_Color kGrassA = {0, 146, 0, 255};
constexpr SDL_Color kRoadA = {0, 0, 0, 255};
constexpr SDL_Color kRoadCity = {60, 60, 68, 255};
constexpr SDL_Color kDashWhite = {235, 235, 235, 255};
constexpr SDL_Color kDashYellow = {235, 215, 40, 255};
constexpr SDL_Color kCurbGray = {170, 170, 170, 255};
constexpr SDL_Color kArchGray = {60, 60, 60, 255};

constexpr std::array<std::array<std::array<uint8_t, 2>, 11>, 20> kSpriteScale = {{
    {{{56, 87}, {34, 52}, {23, 35}, {19, 29}, {15, 24}, {13, 20}, {11, 17}, {10, 15}, {9, 14}, {7, 11}, {6, 9}}},
    {{{7, 14}, {5, 9}, {3, 6}, {2, 5}, {2, 4}, {1, 3}, {1, 3}, {1, 2}, {1, 1}, {1, 1}, {1, 1}}},
    {{{14, 19}, {9, 12}, {7, 8}, {5, 7}, {4, 5}, {3, 4}, {3, 3}, {2, 3}, {2, 3}, {1, 2}, {1, 1}}},
    {{{32, 63}, {20, 38}, {14, 27}, {11, 21}, {9, 17}, {7, 15}, {6, 13}, {5, 11}, {5, 10}, {4, 8}, {3, 6}}},
    {{{30, 21}, {18, 13}, {13, 9}, {10, 7}, {8, 6}, {7, 5}, {6, 4}, {5, 4}, {4, 3}, {4, 3}, {3, 2}}},
    {{{25, 57}, {15, 35}, {11, 25}, {8, 19}, {7, 16}, {6, 14}, {5, 12}, {4, 10}, {4, 9}, {3, 8}, {2, 6}}},
    {{{89, 31}, {54, 19}, {38, 13}, {30, 10}, {24, 8}, {20, 7}, {18, 6}, {15, 5}, {13, 5}, {12, 4}, {9, 3}}},
    {{{19, 9}, {12, 7}, {8, 5}, {7, 3}, {6, 2}, {4, 1}, {3, 1}, {3, 1}, {2, 1}, {1, 1}, {1, 1}}},
    {{{19, 9}, {12, 7}, {8, 5}, {7, 3}, {6, 2}, {4, 1}, {3, 1}, {3, 1}, {2, 1}, {1, 1}, {1, 1}}},
    {{{29, 82}, {17, 49}, {12, 35}, {10, 27}, {8, 22}, {7, 19}, {6, 16}, {5, 14}, {4, 12}, {4, 10}, {3, 8}}},
    {{{68, 70}, {41, 42}, {29, 29}, {22, 23}, {18, 19}, {16, 16}, {14, 14}, {12, 12}, {10, 10}, {8, 8}, {7, 7}}},
    {{{80, 76}, {48, 46}, {34, 32}, {26, 25}, {22, 20}, {18, 17}, {16, 15}, {14, 13}, {12, 11}, {10, 10}, {8, 8}}},
    {{{56, 23}, {34, 14}, {23, 10}, {19, 8}, {15, 6}, {13, 5}, {11, 4}, {10, 4}, {9, 3}, {7, 3}, {6, 2}}},
    {{{64, 23}, {39, 14}, {27, 10}, {21, 8}, {17, 6}, {15, 5}, {13, 4}, {11, 3}, {10, 2}, {8, 2}, {6, 1}}},
    {{{56, 23}, {34, 14}, {23, 10}, {19, 8}, {15, 6}, {13, 5}, {11, 4}, {10, 4}, {9, 3}, {7, 3}, {6, 2}}},
    {{{25, 57}, {15, 35}, {11, 25}, {8, 19}, {7, 16}, {6, 14}, {5, 12}, {4, 10}, {4, 9}, {3, 8}, {2, 6}}},
    {{{25, 54}, {15, 33}, {11, 23}, {8, 18}, {7, 15}, {6, 13}, {5, 11}, {4, 9}, {4, 8}, {3, 7}, {2, 5}}},
    {{{25, 54}, {15, 33}, {11, 23}, {8, 18}, {7, 15}, {6, 13}, {5, 11}, {4, 9}, {4, 8}, {3, 7}, {2, 5}}},
    {{{32, 63}, {20, 38}, {14, 27}, {11, 21}, {9, 17}, {7, 15}, {6, 13}, {5, 11}, {5, 10}, {4, 8}, {3, 6}}},
    {{{32, 63}, {20, 38}, {14, 27}, {11, 21}, {9, 17}, {7, 15}, {6, 13}, {5, 11}, {5, 10}, {4, 8}, {3, 6}}},
}};

int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

// Pick the pre-drawn frame for a given distance row (sr grows with distance,
// 0 = nearest). Frame arrays may be authored smallest- or largest-first, so
// normalize by frame area: nearest always maps to the largest frame.
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

}  // namespace

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

void Racing::DrawGaugeNeedle(SDL_Renderer* renderer, int centerX, int centerY, float value, float minValue, float maxValue) {
    if (!renderer) {
        return;
    }
    constexpr int kNeedleRadius = 14;
    constexpr int kCenterRadius = 5;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kStartAngle = 240.0f * kPi / 180.0f;
    constexpr float kSweepAngle = -240.0f * kPi / 180.0f;
    float t = (value - minValue) / (maxValue - minValue);
    t = std::max(0.0f, std::min(1.0f, t));
    const float angle = kStartAngle + kSweepAngle * t;
    const int startX = centerX + static_cast<int>(kCenterRadius * std::cos(angle));
    const int startY = centerY - static_cast<int>(kCenterRadius * std::sin(angle));
    const int endX = centerX + static_cast<int>(kNeedleRadius * std::cos(angle));
    const int endY = centerY - static_cast<int>(kNeedleRadius * std::sin(angle));
    SDL_SetRenderDrawColor(renderer, 96, 64, 0, 255);
    SDL_RenderDrawLine(renderer, startX, startY, endX, endY);
}

void Racing::DrawNearestNeighborScaled(Bitmap* bitmap, int dstX, int dstY, int dstW, int dstH, bool anchorRight, bool anchorBottom,
                                       int srcX0, int srcY0, int srcW, int srcH) {
    if (!bitmap || dstW <= 0 || dstH <= 0) {
        return;
    }
    SDL_Surface* src = bitmap->GetSurface();
    if (!src || src->w <= 0 || src->h <= 0) {
        return;
    }

    // Default to the whole surface; otherwise sample from the given sub-rect
    // (used to blit a single pre-drawn animation frame from a composite sheet).
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
            const int srcX = srcX0 + (x * srcW) / dstW;
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
    for (int slot = 0; slot < 20; ++slot) {
        // slot is the roadside_sprite_table record index (0 = palm); record N =
        // lib.p asset id 21 + N (see roadside_sprite_table.md).
        const int assetId = RACING_PALM_TREE + slot;
        Bitmap* asset = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, static_cast<AmigaLibAsset>(assetId)});
        if (asset) {
            roadsideSprites[slot] = std::make_unique<Bitmap>(*asset);
        }
        // Prefer pre-drawn frames (a composite sheet with one frame per distance)
        // when the sprite has animation info; otherwise the kSpriteScale fallback runs.
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
}

std::unique_ptr<Bitmap> Racing::LoadOpponentSprite(uint16_t spriteId) {
    byte_vector& libFile = state.GetAssets().GetLibPData();
    BitmapParser bp;
    auto imageEntries = state.GetAssets().GetImageEntries();
    BitmapEntry_t entry = imageEntries.second[spriteId];
    std::vector<uint8_t> data = bp.ReadImage(libFile, entry, spriteId, imageEntries.first);

    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, spriteId);
    std::vector<uint16_t> opponentCarPalette = GetStandardCarColor(opponentColor);
    if (opponentCarPalette.size() > 7) {
        palData[7] = opponentCarPalette[7];
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
        default:
            opponentCarType = 0;
            LoadOpponentCarSprites();
            break;
    }
}

srrace::CarSetup Racing::BuildCarSetupFromCar(const Car* car, bool isPlayer) const {
    srrace::CarSetup setup;
    setup.weightClass = 4;
    setup.driverSkill = isPlayer ? 130 : 100;

    if (!car) {
        setup.transClass = automaticOverride ? 0 : 2;
        return setup;
    }

    switch (car->GetCarburator().partType.carburetorType) {
        case CARB_2BARREL: setup.carbClass = 0; break;
        case CARB_4BARREL: setup.carbClass = 1; break;
        case CARB_4BARREL_RACING: setup.carbClass = 2; break;
        default: setup.carbClass = 0; break;
    }

    switch (car->GetGearbox().partType.transmissionType) {
        case TRANS_AUTO_GM:
        case TRANS_AUTO_FORD:
        case TRANS_AUTO_CHRYSLER:
            setup.transClass = 0;
            break;
        case TRANS_3SPD_GM:
        case TRANS_3SPD_FORD:
        case TRANS_3SPD_CHRYSLER:
            setup.transClass = 1;
            break;
        case TRANS_4SPD_GM:
        case TRANS_4SPD_FORD:
        case TRANS_4SPD_CHRYSLER:
        case TRANS_4SPD_UNIVERSAL:
            setup.transClass = 2;
            break;
        case TRANS_RACING_GM:
        case TRANS_RACING_FORD:
        case TRANS_RACING_CHRYSLER:
            setup.transClass = 3;
            break;
        default:
            setup.transClass = 2;
            break;
    }

    if (automaticOverride) {
        setup.transClass = 0;
    } else if (setup.transClass == 0) {
        setup.transClass = 2;
    }

    switch (car->GetEngine().engineType) {
        case CarEngineGM_V6:
        case CarEngineFord_V6:
        case CarEngineChrysler_V6:
            setup.engineClass = 1;
            break;
        default:
            setup.engineClass = 2;
            break;
    }

    switch (car->GetManifold().partType.manifoldType) {
        case MANIFOLD_STOCK: setup.manifoldClass = 0; break;
        case MANIFOLD_PERFORMANCE: setup.manifoldClass = 3; break;
        case MANIFOLD_RACING: setup.manifoldClass = 4; break;
        default: setup.manifoldClass = 0; break;
    }

    switch (car->GetWheelType()) {
        case STOCK: setup.tireClass = 0; break;
        case BRAND: setup.tireClass = 1; break;
        case RACING: setup.tireClass = 2; break;
        default: setup.tireClass = 0; break;
    }

    setup.tuneOffset = ClampInt(std::abs(car->GetTuneLevel() - 9), 0, 8);
    setup.carbWear = ClampInt(car->GetCarburator().wear, 0, 10000);
    setup.engineWear = ClampInt(car->GetEngine().wear, 0, 10000);
    setup.transWear = ClampInt(car->GetGearbox().wear, 0, 10000);
    setup.tireWear = ClampInt(car->GetTires().wear, 0, 10000);
    setup.bumpersStripped = car->IsBumpersStripped();
    return setup;
}

void Racing::InitializeRaceSimulation() {
    trackVariant = rng.Next(10);
    srrace::RoadLayoutInit(track, rng);
    srrace::GenerateTrackShapes(track, trackVariant);

    const Car* playerCar = state.GetCurrentCar();
    srrace::CarSetup playerSetup = BuildCarSetupFromCar(playerCar, true);

    srrace::CarSetup npcSetup;
    Opponent* opponent = state.GetCurrentOpponent();
    if (opponent && opponent->car) {
        npcSetup = BuildCarSetupFromCar(opponent->car, false);
    } else {
        npcSetup = playerSetup;
        npcSetup.driverSkill = 100;
    }

    sim.Init(&track, ToRaceType(currentTrackType), playerSetup, npcSetup, static_cast<uint32_t>(SDL_GetTicks()));
    simInitialized = true;
    fixedAccumulator = 0.0f;
    prevSpaceDown = false;
    pendingShift = false;
    firstPlayerFinishTick = -1;
    firstNpcFinishTick = -1;
    simTickCounter = 0;
    raceOutcome = RaceOutcome::Ongoing;
    bShowCrashOverlay = false;
    crashHandled = false;
    crashOverlayEndMs = 0;
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
    engineSoundStarted = false;
    InitializeRaceSimulation();
}

void Racing::SetTransmissionType(bool automatic) {
    automaticOverride = automatic;
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

void Racing::InitializeOpponent() {
    Opponent* opponent = state.GetCurrentOpponent();
    if (!opponent || !opponent->car) {
        opponentActive = false;
        return;
    }

    opponentActive = true;
    opponentColor = opponent->car->GetColor();
    CarVendor_t vendor = opponent->car->GetModel().vendor;
    switch (vendor) {
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
    return sim.Player().rpm;
}

int16_t Racing::GetSteerDisplay() const {
    return sim.Player().slideVel;
}

std::string Racing::GetRaceEndReason() const {
    const uint16_t flags = sim.Player().endFlags;
    if (flags & srrace::END_WALL_HIT)      return "CRASHED";
    if (flags & srrace::END_ENGINE_BLOWN)  return "ENGINE BLEW";
    if (flags & srrace::END_TRANS_DROPPED) return "TRANSMISSION DROPPED";
    if (flags & srrace::END_STALLED)       return "JUMPED THE START";
    if (raceOutcome == RaceOutcome::PlayerWon)  return "YOU WIN!";
    if (raceOutcome == RaceOutcome::PlayerLost) return "YOU LOSE";
    return "RACE OVER";
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

bool Racing::ShouldShowCrashOverlay() const {
    return bShowCrashOverlay;
}

void Racing::HideCrashOverlay() {
    bShowCrashOverlay = false;
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
    const int curveAtCar = p.curveAtCar;

    for (int k = 0; k < kForwardRows; ++k) {
        int d = 4;
        if (k > 0) {
            d = 12 + 8 * (k - 1) - p.scrollAcc;
            d = std::max(4, d);
        }
        // BuildForwardRowTables (0x23abbe): the horizontal projection (road_d,
        // ProjectForwardX) reads the elev[] array (A4+0x1098) and the vertical
        // projection (row_c, ProjectForwardY) reads the curve[] array (A4+0x7d8).
        // In this engine elev[] is what bends the road left/right (and drives the
        // lateral drift); curve[] is what moves it up/down. Do NOT swap these.
        const int seg = ClampInt(segBase + k + 1, 0, kTrackSegments - 1);
        const int roadDelta = p.heading - track.elev[seg];
        const int rowCurveDelta = curveAtCar - track.curve[seg];
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
        row.y = ClampInt(kHorizonY + 400 / d + 4 * rowCurveDelta, kClipTop, kClipBottom + 30);
        // Overhead gantry crossbar height (RenderOverheadArch 0x237bf4):
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
    for (int y = 0; y <= kClipBottom; ++y) {
        for (int x = 0; x < GAME_WIDTH; ++x) {
            PutPixel(pixels, x, y, kSkyColor);
        }
    }

    const srrace::CarState& p = sim.Player();
    cloudOffsetAccum += static_cast<float>(p.heading) * static_cast<float>(p.speed) * frameSeconds * 0.005f;
    mountainOffsetAccum += static_cast<float>(p.heading) * static_cast<float>(p.speed) * frameSeconds * 0.0025f;

    if (cloudSprite && cloudSprite->GetSurface()) {
        SDL_Surface* src = cloudSprite->GetSurface();
        const int baseOffset = ((static_cast<int>(cloudOffsetAccum) % src->w) + src->w) % src->w;
        for (int pass = 0; pass < 2; ++pass) {
            const int drawX = -baseOffset + pass * src->w;
            for (int y = 0; y < src->h; ++y) {
                for (int x = 0; x < src->w; ++x) {
                    const int px = drawX + x;
                    if (px < 0 || px >= GAME_WIDTH || y < 0 || y >= GAME_RACING_HEIGHT) {
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

    if (mountainSprite && mountainSprite->GetSurface()) {
        SDL_Surface* src = mountainSprite->GetSurface();
        const int baseOffset = ((static_cast<int>(mountainOffsetAccum) % src->w) + src->w) % src->w;
        const int startY = 61;
        for (int pass = 0; pass < 2; ++pass) {
            const int drawX = -baseOffset + pass * src->w;
            for (int y = 0; y < src->h; ++y) {
                const int py = startY + y;
                if (py < 0 || py >= GAME_RACING_HEIGHT) {
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

    for (int y = kClipTop; y <= kClipBottom; ++y) {
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

        const int yStart = ClampInt(nearRow.y - 1, kClipTop, kClipBottom);
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
            const SDL_Color roadColor = city ? kRoadCity : kRoadA;
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
        const int yStart = ClampInt(row0.y, kClipTop, kClipBottom);
        const int dy = std::max(1, row0.y - row1.y);
        const uint16_t nearFlags = track.flags[row0.seg];
        const bool nearCity = (nearFlags & 0x0001) != 0;
        const SDL_Color nearRoadColor = nearCity ? kRoadCity : kRoadA;
        for (int y = yStart; y <= kClipBottom; ++y) {
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

    if (lastVisible == kForwardRows - 1) {
        const int horizonFillEnd = ClampInt(rows[lastVisible].y, kClipTop, kClipBottom);
        for (int y = kClipTop; y < horizonFillEnd; ++y) {
            for (int x = 0; x < GAME_WIDTH; ++x) {
                PutPixel(pixels, x, y, kGrassA);
            }
        }
    } else {
        const int skyEnd = ClampInt(rows[lastVisible].y, kClipTop, kClipBottom);
        for (int y = kClipTop; y < skyEnd; ++y) {
            for (int x = 0; x < GAME_WIDTH; ++x) {
                PutPixel(pixels, x, y, kSkyColor);
            }
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
        PutPixel(pixels, left, y, kArchGray);
        PutPixel(pixels, left + 1, y, kArchGray);
        PutPixel(pixels, right, y, kArchGray);
        PutPixel(pixels, right - 1, y, kArchGray);
    }

    // Thin crossbar (the arch is now tall; a quarter-height slab would look wrong).
    const int barHeight = ClampInt((yBottom - yTop) / 20, 2, 8);
    for (int y = yTop; y < std::min(GAME_RACING_HEIGHT, yTop + barHeight); ++y) {
        for (int x = std::max(0, left); x <= std::min(GAME_WIDTH - 1, right); ++x) {
            PutPixel(pixels, x, y, kArchGray);
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

    // These sign assets are composite sheets of pre-drawn scale frames. Step
    // through them by distance (same as roadside objects) so the sign grows as we
    // approach, blitting the chosen frame at native size rather than scaling the
    // whole sheet.
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
            // Blit the distance-appropriate frame at its native size (no scaling)
            // straight from the composite sheet.
            const SpriteRect& fr = anim->frames[SelectFrameIndexForDistance(anim, sr)];
            w = fr.w;
            h = fr.h;
            srcX0 = fr.x;
            srcY0 = fr.y;
            srcW = fr.w;
            srcH = fr.h;
        } else {
            // TODO: temporary fallback. Nearest-neighbor scaling of a single bitmap
            // is not how the original game draws scenery. Once every roadside object
            // has pre-drawn frames mapped (author them in RodEdit's composite editor
            // and add SPRITE_ANIMATIONS[] entries in libp.h), delete this branch, the
            // kSpriteScale table, and the scaling path of DrawNearestNeighborScaled.
            w = kSpriteScale[slot][sr][0];
            h = kSpriteScale[slot][sr][1];
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
            if (L & 0x0001) drawSlot(1, k, -1, city);
            if (L & 0x0004) drawSlot(7, k, -1, false);
            if (L & 0x0008) drawSlot(8, k, -1, false);
            if (L & 0x0040) drawSlot(city ? 2 : 10, k, -1, false);
            if (L & 0x0080) drawSlot(11, k, -1, false);

            if (R & 0x0004) drawSlot(7, k, +1, false);
            if (R & 0x0008) drawSlot(8, k, +1, false);
            if (R & 0x0040) drawSlot(city ? 2 : 10, k, +1, false);
            if (R & 0x0080) drawSlot(11, k, +1, false);
            if (R & 0x0020) drawSlot(0, k, +1, false);
            if (R & 0x0100) drawSlot(4, k, +1, false);
            if (R & 0x0800) drawSlot(3, k, +1, false);
            if (R & 0x1000) drawSlot(6, k, +1, false);
            if (R & 0x2000) drawSlot(18, k, +1, false);
            if (R & 0x4000) drawSlot(19, k, +1, false);
            if (R & 0x0001) drawSlot(1, k, +1, city);

            if (F & 0x0002) drawSlot(9, k, +1, false);
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

void Racing::DrawNpcCar(int lastVisible) {
    if (flybyMode) {
        if (!flybyNpcVisible) {
            return;
        }
    } else if (!opponentActive) {
        return;
    }

    const int delta = static_cast<int>(sim.Npc().segPos - sim.Player().segPos);
    if (delta < 0 || delta > 14) {
        return;
    }

    const int k = ClampInt(delta, 0, std::min(lastVisible, 14));
    const RowProjection& row = rows[k];
    int frameIndex = ClampInt(k / 2, 0, 6);
    Bitmap* sprite = nullptr;
    switch (opponentCarType) {
        case 0: sprite = opponentChevyRear[frameIndex].get(); break;
        case 1: sprite = opponentChryslerRear[frameIndex].get(); break;
        case 2: sprite = opponentCopRear[frameIndex].get(); break;
        default: sprite = opponentChevyRear[frameIndex].get(); break;
    }
    if (!sprite || !sprite->GetSurface()) {
        return;
    }

    const srrace::CarState& p = sim.Player();
    SDL_Surface* src = sprite->GetSurface();
    const int x = ProjectX(p, row.distance, row.roadDelta, sim.Npc().lateral);
    // Blit the pre-drawn LOD frame at its native size (no scaling between frames);
    // distance is conveyed by which frame is picked, not by resizing one bitmap.
    DrawNearestNeighborScaled(sprite, x - src->w / 2, row.y, src->w, src->h, false, true);
}

void Racing::DrawRearviewMirror() {
    const int mirrorX = 242;
    const int mirrorY = 9;
    const int mirrorW = 76;
    const int mirrorH = 21;
    for (int y = mirrorY; y < mirrorY + mirrorH; ++y) {
        for (int x = mirrorX; x < mirrorX + mirrorW; ++x) {
            PutPixel(pixels, x, y, {0, 0, 0, 255});
        }
    }

    if (!rearviewMirrorSprite || !rearviewMirrorSprite->GetSurface()) {
        return;
    }
    SDL_Surface* src = rearviewMirrorSprite->GetSurface();
    const int srcX0 = 1;
    const int srcY0 = 0;
    const int srcW = 76;
    const int srcH = 11;
    const int dstY = mirrorY + 6;
    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            const int dstX = mirrorX + x;
            const int py = dstY + y;
            if (dstX < 0 || dstX >= GAME_WIDTH || py < 0 || py >= GAME_RACING_HEIGHT) {
                continue;
            }
            const int sx = srcX0 + x;
            const int sy = srcY0 + y;
            Uint32 srcPixel = 0;
            const Uint8* srcPtr = static_cast<const Uint8*>(src->pixels) + sy * src->pitch + sx * src->format->BytesPerPixel;
            std::memcpy(&srcPixel, srcPtr, src->format->BytesPerPixel);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 255;
            SDL_GetRGBA(srcPixel, src->format, &r, &g, &b, &a);
            if (a > 0) {
                PutPixel(pixels, dstX, py, {r, g, b, a});
            }
        }
    }

    // Show the opponent in the mirror when it is behind us (originally the mirror
    // renders the rear view with the NPC in it; here we blit the "behind" sprite
    // into the glass, growing as it closes). Full rear-view road pass is TODO.
    if (opponentActive) {
        const int delta = static_cast<int>(sim.Npc().segPos - sim.Player().segPos);
        if (delta < 0 && delta >= -8) {
            const int idx = ClampInt((-delta) / 2, 0, 3);
            Bitmap* behind = nullptr;
            switch (opponentCarType) {
                case 1: behind = opponentChryslerBehind[idx].get(); break;
                case 2: behind = opponentCopBehind[idx].get(); break;
                default: behind = opponentChevyBehind[idx].get(); break;
            }
            if (behind && behind->GetSurface()) {
                SDL_Surface* bs = behind->GetSurface();
                // Native LOD frame (no scaling); crop to fit the mirror glass.
                const int w = std::min(bs->w, mirrorW - 2);
                const int h = std::min(bs->h, mirrorH - 2);
                const int cx = mirrorX + mirrorW / 2 - w / 2;
                const int cy = mirrorY + mirrorH - h - 1;
                DrawNearestNeighborScaled(behind, cx, cy, w, h, false, false, 0, 0, w, h);
            }
        }
    }
}

void Racing::DrawHud() {
    if (!hud) {
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
    char infoText[128];
    const int distanceToFinish = std::max(sim.FinishSegment() - static_cast<int>(sim.Player().segPos), 0);
    std::snprintf(infoText, sizeof(infoText), "Speed: %3d mph  RPM: %4d  To finish: %d",
                  static_cast<int>(sim.Player().speed),
                  static_cast<int>(sim.Player().rpm),
                  distanceToFinish);
    hud->Clear();
    hud->AddText(6, 0, gearText);
    hud->AddText(68, 0, infoText);
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
    if (flags & 0x0200) out += "upHill ";
    if (flags & 0x0400) out += "dnHill ";
    if (flags & 0x0800) out += "narrowWarn ";
    if (flags & 0xf000) out += "arch ";
    if (out.empty()) {
        out = "straight";
    }
    return out;
}

void Racing::ApplyFlybyCamera() {
    const int seg = ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1);
    const int16_t curveAtCar = track.curve[seg];
    sim.SetCameraState(flybySeg, flybyScrollAcc, flybyHeading, flybyLateral, curveAtCar);
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
    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
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
    if (!flybyMode) {
        return lines;
    }

    const int seg = ClampInt(static_cast<int>(flybySeg), 0, srrace::TRACK_SEGMENTS - 1);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "FLYBY seg %d +%d/8  var %d  %s",
                  seg, static_cast<int>(flybyScrollAcc), trackVariant,
                  currentTrackType == SR_TRACK_DRAG_RACE ? "drag" : "road");
    lines.push_back(buf);

    std::snprintf(buf, sizeof(buf), "curve %d elev %d  heading %d  lat %d",
                  static_cast<int>(track.curve[seg]),
                  static_cast<int>(track.elev[seg]),
                  static_cast<int>(flybyHeading),
                  static_cast<int>(flybyLateral));
    lines.push_back(buf);

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

    lines.push_back("[ ] step  shift+[ ] seg  ctrl+[ ] +/-10seg");
    lines.push_back("=/- heading  ,/. lateral  Home/End start/finish");
    lines.push_back("PgUp/Dn +/-50seg  N toggle NPC  F9 exit flyby");
    lines.push_back("P save road PNG (no cockpit)");
    return lines;
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
    if (raceOutcome != RaceOutcome::Ongoing) {
        return;
    }

    // A crash / blown engine / dropped transmission / false start ends the race
    // as a loss (original: any A4+0x4bc6 failure bit exits the frame loop). Show
    // the crash overlay for a wall hit before settling the outcome.
    if (sim.PlayerCrashed()) {
        if (!crashHandled) {
            crashHandled = true;
            bShowCrashOverlay = true;
            crashOverlayEndMs = SDL_GetTicks() + 3000;
        }
        raceOutcome = RaceOutcome::PlayerLost;
        return;
    }

    if (sim.PlayerFinished()) {
        if (sim.NpcAi().gaveUp) {
            raceOutcome = RaceOutcome::PlayerWon;
            return;
        }
        if (sim.NpcFinished() && firstNpcFinishTick >= 0 && firstNpcFinishTick < firstPlayerFinishTick) {
            raceOutcome = RaceOutcome::PlayerLost;
        } else {
            raceOutcome = RaceOutcome::PlayerWon;
        }
        return;
    }

    // The opponent crossed the line first and the player has not finished -> loss.
    if (sim.NpcFinished() && !sim.NpcAi().gaveUp) {
        raceOutcome = RaceOutcome::PlayerLost;
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

    if (flybyMode) {
        UpdateFlybyMovement(fElapsedTime);
    } else {
        fixedAccumulator += std::max(0.0f, fElapsedTime);
        fixedAccumulator = std::min(fixedAccumulator, kFixedStepSeconds * 2.0f);

        const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
        uint16_t baseInput = 0;
        const bool up = keyboard && keyboard[SDL_SCANCODE_UP];
        const bool down = keyboard && keyboard[SDL_SCANCODE_DOWN];
        const bool left = keyboard && keyboard[SDL_SCANCODE_LEFT];
        const bool right = keyboard && keyboard[SDL_SCANCODE_RIGHT];
        const bool space = keyboard && keyboard[SDL_SCANCODE_SPACE];

        if (up) baseInput |= srrace::INPUT_THROTTLE;
        if (down && !up) baseInput |= srrace::INPUT_BRAKE;
        if (left) baseInput |= srrace::INPUT_STEER_LEFT;
        if (right) baseInput |= srrace::INPUT_STEER_RIGHT;
        if (space && !prevSpaceDown) {
            pendingShift = true;
        }
        prevSpaceDown = space;

        if (fixedAccumulator >= kFixedStepSeconds) {
            uint16_t inputBits = baseInput;
            if (pendingShift) {
                inputBits |= srrace::INPUT_SHIFT;
                pendingShift = false;
            }
            sim.Tick(inputBits);
            ++simTickCounter;

            if (firstPlayerFinishTick < 0 && (sim.Player().endFlags & srrace::END_FINISHED)) {
                firstPlayerFinishTick = simTickCounter;
            }
            if (firstNpcFinishTick < 0 && (sim.Npc().endFlags & srrace::END_FINISHED)) {
                firstNpcFinishTick = simTickCounter;
            }
            fixedAccumulator -= kFixedStepSeconds;
        }

        HandleOutcomeAfterTicks();
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
    DrawRoadsideAndArchSprites(lastVisibleRow);
    DrawNpcCar(lastVisibleRow);
    DrawRearviewMirror();

    SDL_UnlockSurface(pixels);
    if (flybyMode) {
        DrawFlybyHud(lastVisibleRow);
    } else {
        DrawHud();
    }
}

void Racing::DrawTachometerNeedleToRenderer(SDL_Renderer* renderer) {
    DrawGaugeNeedle(renderer, 145, 129, static_cast<float>(sim.Player().rpm), 1500.0f, 7000.0f);
}

void Racing::DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer) {
    DrawGaugeNeedle(renderer, 196, 129, static_cast<float>(sim.Player().speed), 0.0f, 140.0f);
}

SDL_Surface* Racing::GetSurface() {
    return pixels;
}
