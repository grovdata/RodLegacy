/*
 * RodLegacy - The race view: Racing renders the road and cars, reads input and steps the race simulation.
 *
 * Created: 2024-04-28
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_manager.h"
#include "debug/debug_ui.h"
#include "gfx/bitmap.h"
#include "parsers/carcolor.h"
#include "parsers/libp.h"
#include "race/race_end_message.h"
#include "race/race_sim.h"
#include "race/sr2_gauge.h"
#include "race/aqueduct.h"
#include "race/track_gen.h"
#include "race/race_types.h"
#include "ui/hud.h"

class Car;

// Door mirror glass per row, screen coordinates. The glass in RACING_INTERIOR_1 is a trapezoid, wider on its
// upper rows.
struct SideMirrorRow { int16_t left, right; };
inline constexpr int kSideMirrorTop  = 101;   // screen y of the first row
inline constexpr int kSideMirrorSrcX = 242;   // roof mirror glass, screen coords
inline constexpr int kSideMirrorSrcY = 27;
inline constexpr SideMirrorRow kSideMirrorGlass[] = {
    {7, 34},
    {7, 35}, {7, 35}, {7, 35}, {7, 35},
    {7, 34}, {7, 34}, {7, 34}, {7, 34}, {7, 34}, {7, 34},
    {7, 33}, {7, 33}, {7, 33}, {7, 33},
    {7, 32},
};
inline constexpr int kSideMirrorRows = static_cast<int>(std::size(kSideMirrorGlass));

// Inner end first, offsets from the dial origin.
struct NeedleLine { int8_t x0, y0, x1, y1; };

enum class RaceOutcome {
    Ongoing,
    PlayerWon,
    PlayerLost,
};

// Enough for Street Rod 2's 23 roadside_sprite_table records; Street Rod has 20.
constexpr int kRoadsideSlots = 23;
constexpr int kSpriteScaleFrameCount = 11;

class Racing {
private:
    struct RowProjection {
        int y = 0;
        int leftX = 0;
        int rightX = 0;
        int centerX = 0;
        int shoulderLX = 0;
        int shoulderRX = 0;
        int archY = 0;
        int distance = 4;
        int seg = 0;
        int roadDelta = 0;  // heading - elev[seg] for ProjectForwardX
        // Mulholland scenery, unclamped: road y, the 1900 row (bridge tops), wall tops and verge x.
        int yRaw = 0;
        int yTop = 0;
        int yMid = 0;   // the 1950 row, half way up: guard rail posts end here
        int wallLY = 0;
        int wallRY = 0;
        int vergeLX = 0;
        int vergeRX = 0;
    };

    HUD* hud = nullptr;
    SDL_Surface* pixels = nullptr;
    AudioManager* audioManager = nullptr;
    bool engineSoundStarted = false;

    SrTrackType currentTrackType = SR_TRACK_ROAD_RACE;
    // By race end State's pending bet is settled, so the message uses these.
    int raceBetAmount = 0;
    bool raceIsPinkSlip = false;
    bool automaticOverride = false;
    bool simInitialized = false;
    bool bShowCrashOverlay = false;
    bool crashHandled = false;
    int fallStep = 0;   // jdraw.c FUN_00240d9a, 0..8
    int FallDrop() const { return fallStep * (fallStep + 1) / 2; }
    float fixedAccumulator = 0.0f;
    bool prevSpaceDown = false;
    bool pendingShift = false;
    int firstPlayerFinishTick = -1;
    int firstNpcFinishTick = -1;
    int simTickCounter = 0;
    bool cruiseMode = false;   // drive cutscene, not a race
    std::unique_ptr<Bitmap> cruiseSignSprite;
    const SpriteAnimationInfo* cruiseSignAnim = nullptr;
    int32_t speedSumMph = 0;
    // The original refires every tick; our audio only queues whole samples, so retrigger once one has played out.
    int tireScreechCooldownTicks = 0;
    // Same retrigger scheme for the cop siren.
    int copSirenCooldownTicks = 0;

    srrace::TrackData track {};
    srrace::RaceRng rng;
    int trackVariant = 0;
    srrace::RaceSim sim;
    // What the renderer draws: the sim's cars, or with smooth racing a blend of the last two steps.
    srrace::CarState prevPlayer, prevNpc, viewPlayer, viewNpc;
    bool havePrevStep = false;
    std::array<RowProjection, 15> rows {};

    // The row that sits on the horizon, not the bitmap's lowest opaque row. -1 = uncomputed.
    int mountainBaseRow = -1;

    std::array<std::unique_ptr<Bitmap>, kRoadsideSlots> roadsideSprites {};
    std::array<const SpriteAnimationInfo*, kRoadsideSlots> roadsideAnim {};
    // Per slot, per distance row {srcX, srcY, w, h} from the game's executable, plus each slot's lib.p id.
    // Street Rod 2's ids are irregular, so the id comes from the record and not the slot number.
    std::array<std::array<SpriteRect, 11>, kRoadsideSlots> roadsideFrames {};
    std::array<int, kRoadsideSlots> roadsideAssetId {};
    int roadsideSlotCount = 0;
    SrGame roadsideLoadedFor = SrGame::SR1;
    void LoadRoadsideForGame(SrGame game);
    std::unique_ptr<Bitmap> mountainSprite;
    std::unique_ptr<Bitmap> cloudSprite;
    std::unique_ptr<Bitmap> startLightSprite;
    // Held full during staging, then retracts by 4/frame at launch.
    int startLightHeight = 32;
    // Street Rod 2 slides its light out sideways instead: 0 to 88 in steps of
    // 8 (jlights.c 0x244a48 masks the counter with 0xfff8 and stops past 88).
    int sr2LightSlide = 0;
    int sr2StarterSeg = -1;   // where the starter stands, A4+0x22bc
    void DrawStarterInMirror(int r, int x, int y, bool aqueduct, const SDL_Rect& clip);
    std::unique_ptr<Bitmap> rearviewMirrorSprite;
    std::unique_ptr<Bitmap> signCountyLineSprite;
    std::unique_ptr<Bitmap> signRoute69Sprite;
    std::unique_ptr<Bitmap> signExit48Sprite;

    std::unique_ptr<Bitmap> opponentChevyFrontSide;
    std::unique_ptr<Bitmap> opponentChevyRearSide;
    std::unique_ptr<Bitmap> opponentChevyRear[7];
    std::unique_ptr<Bitmap> opponentChevyBehind[4];
    std::unique_ptr<Bitmap> opponentChryslerFrontSide;
    std::unique_ptr<Bitmap> opponentChryslerRearSide;
    std::unique_ptr<Bitmap> opponentChryslerRear[7];
    std::unique_ptr<Bitmap> opponentChryslerBehind[4];
    std::unique_ptr<Bitmap> opponentCopFrontSide;
    std::unique_ptr<Bitmap> opponentCopRearSide;
    std::unique_ptr<Bitmap> opponentCopRear[7];
    std::unique_ptr<Bitmap> opponentCopBehind[4];
    std::unique_ptr<Bitmap> opponentOldsmobileFrontSide;
    std::unique_ptr<Bitmap> opponentOldsmobileRearSide;
    std::unique_ptr<Bitmap> opponentOldsmobileRear[7];
    std::unique_ptr<Bitmap> opponentOldsmobileBehind[4];
    std::unique_ptr<Bitmap> opponentChevy2FrontSide;
    std::unique_ptr<Bitmap> opponentChevy2RearSide;
    std::unique_ptr<Bitmap> opponentChevy2Rear[7];
    std::unique_ptr<Bitmap> opponentChevy2Behind[4];
    std::unique_ptr<Bitmap> opponentCorvette63FrontSide;
    std::unique_ptr<Bitmap> opponentCorvette63RearSide;
    std::unique_ptr<Bitmap> opponentCorvette63Rear[7];
    std::unique_ptr<Bitmap> opponentCorvette63Behind[4];
    std::unique_ptr<Bitmap> opponentFordFrontSide;
    std::unique_ptr<Bitmap> opponentFordRearSide;
    std::unique_ptr<Bitmap> opponentFordRear[7];
    std::unique_ptr<Bitmap> opponentFordBehind[4];

    bool opponentActive = false;
    int opponentCarType = 0;
    // -1 = no swap in effect.
    int opponentCarTypeBeforeCop = -1;
    // Street Rod 2: 13 sprites by view state for the opponent's body style, and the cop set. Type 2 is the cop,
    // any other type is the style set.
    // Each state at the seven Aqueduct tilts; tilt 3 is the level frame every other race uses. The offsets put
    // the frame's bottom-centre anchor on the projected point.
    std::unique_ptr<Bitmap> sr2Opp[13][srrace::AQU_OPP_TILTS];
    int8_t sr2OppXOff[13][srrace::AQU_OPP_TILTS] = {};
    int8_t sr2OppYOff[13][srrace::AQU_OPP_TILTS] = {};
    std::unique_ptr<Bitmap> sr2Cop[13];
    int sr2OppStyle = 0;
    // An SR2 race against a car from Street Rod's set uses Street Rod's opponent sprites, which have no tilts.
    bool sr1BodyInSr2 = false;
    bool sr1BodyBeforeCop = false;
    StandardCarColor opponentColor = CarColorRed;
    RaceOutcome raceOutcome = RaceOutcome::Ongoing;

    bool debugFeaturesEnabled = false;
    bool debugInvulnerable = false;
    bool flybyMode = false;
    bool flybyNpcVisible = false;
    int32_t flybySeg = srrace::RACE_START_SEG;
    int16_t flybyScrollAcc = 0;
    int16_t flybyHeading = 0;
    int16_t flybyLateral = 2560;
    bool opponentActiveBeforeFlyby = false;
    int lastVisibleRow = -1;

    std::vector<NeedleLine> needleLines;
    srrace::Sr2Gauges sr2Gauges;
    int sr2DashboardVariant = 2;   // which of SR2's three dial layouts the dashboard art matches
    void DrawSr2Needle(SDL_Renderer* renderer, int type, int gaugeValue);

    void PutPixel(SDL_Surface* s, int x, int y, SDL_Color c);
    void DrawGaugeNeedle(SDL_Renderer* renderer, int originX, int originY, int gaugeValue);
    void LoadSpriteBitmaps();
    void DrawTownWalls(int lastVisible);
    void DrawRearviewMirror();
    void DrawSideMirror();
    std::unique_ptr<Bitmap> LoadOpponentSprite(uint16_t spriteId);
    void LoadOpponentCarSprites();
    void BuildSr1TiltFrames();
    void LoadSr2OpponentSprites();
    void InitializeRaceSimulation();
    srrace::CarSetup BuildCarSetupFromCar(const Car* car, bool isPlayer) const;
    int ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const;
    int ProjectY(int d, int elevDelta, int lift = 100) const;
    [[nodiscard]] bool IsMulholland() const;
    [[nodiscard]] SDL_Color Sr2Pen(int pen, SDL_Color fallback) const;
    void FillPolygon(const int* xs, const int* ys, int n, SDL_Color c);
    void RenderMulhollandBackdrop();
    void DrawMulhollandWalls(int r, bool farBridgeEnd);
    void DrawMulhollandBridge(int r);
    void DrawMulhollandRail(int r);
    // Segment of the farthest drop start in view; the rail's posts alternate from it. Kept across frames.
    int railPhaseSeg = 0;
    int BuildRowTableAndGetLastVisible();
    void UpdateRenderView();
    void RenderSky();
    void RenderClouds(int horizonY);
    void RenderMountains(int horizonY);
    void DrawStartLight();
    void DrawSr2StartLight();
    [[nodiscard]] Bitmap* Sr2Asset(int id) const;
    void DrawStopLights(int k);
    [[nodiscard]] int StopLightLamp(int column) const;
    // onlyBand >= 0 draws just the band between rows onlyBand and onlyBand + 1, without the background.
    void RenderRoad(int lastVisible, int onlyBand = -1);
    bool mulhollandFarBridgeEnd = false;
    // 1:1 blit into the road surface, clipped. The source rect defaults to the whole bitmap.
    void DrawSprite(Bitmap* bitmap, int dstX, int dstY, bool anchorRight, bool anchorBottom,
                    int srcX0 = 0, int srcY0 = 0, int srcW = -1, int srcH = -1);

    // Draws the concrete channel instead of the road, driven by the live sim.
    void RenderAqueduct();
    std::unique_ptr<Bitmap> hazardSprites[srrace::HazardKindCount];
    srrace::AqueductTrack aqueductTrack{};
    srrace::AqueductFrame aqueductFrame{};
    // The bank and concrete pen of this frame's channel, so the NPC car and the mirror match it.
    srrace::AqueductBank aqueductBank{};
    int aqueductBankAngle = 0;
    SDL_Color aqueductConcrete = {153, 153, 153, 255};
    void DrawAqueductMirror();
    void DrawAqueductOpponent(int viewState);
    void DrawAqueductShape(const srrace::AqueductObstacle& o, const SDL_Rect& clip);
    void DrawAqueductLine(const srrace::AqueductLine& line, SDL_Color c, const SDL_Rect& clip);
    srrace::AqueductBank DrawAqueductPass(const srrace::AqueductRow* rows, int objectRows, const uint16_t* flags,
                                          int flagBase, const SDL_Rect& clip, bool hitTest);
    SDL_Color AqueductPen(int idx, SDL_Color fallback) const;
    [[nodiscard]] SDL_Color DashColorFor(uint16_t flags) const;
    bool aqueductPreview = false;
    bool cruiseStarting = false;
    bool aqueductPreviewApplied = false;
    // The channel's rows sit around 2000, so the camera must be near that height; the original's value is not traced.
    int aqueductCameraHeight = 1900;

public:
    // The channel replaces the road with its own track, walls and renderer.
    void SetAqueductRace(bool on) { aqueductPreview = on; }
    bool IsAqueductRace() const { return aqueductPreview; }
    bool& AqueductPreviewRef() { return aqueductPreview; }
    int& AqueductCameraHeightRef() { return aqueductCameraHeight; }

private:
    void DrawRoadsideAndArchSprites(int lastVisible);
    void DrawOverheadArch(const RowProjection& row, uint16_t flags, int sr);
    Bitmap* OpponentSpriteForState(int state) const;
    void DrawNpcCar(int lastVisible);
    void DrawHud();
    void DrawFlybyHud(int lastVisible);
    void HandleOutcomeAfterTicks();
    void ApplyFlybyCamera();
    void UpdateFlybyMovement(float frameSeconds);
    void FlybyScroll(int scrollDelta);
    void FlybyJumpSegments(int segDelta);
    static std::string FormatSegmentFlags(uint16_t flags);

public:
    Racing();
    Racing(AudioManager* audioMgr);
    ~Racing();

    void Draw(float fElapsedTime, SDL_Event* event);
    SDL_Surface* GetSurface();

    void DrawTachometerNeedleToRenderer(SDL_Renderer* renderer);
    void DrawSpeedometerNeedleToRenderer(SDL_Renderer* renderer);

    void LoadTrackData(const std::vector<uint8_t>& binaryData, SrTrackType trackType = SR_TRACK_ROAD_RACE);
    void SetTrackType(SrTrackType trackType);
    SrTrackType GetTrackType() const;

    void InitializeOpponent();

    float GetSpeed() const;

    int16_t GetCurrentGear() const;
    // >0 right, <0 left, up to ~24. Decays after release so the wheel re-centers gradually.
    int16_t GetSteerDisplay() const;

    // Call before ResetRaceOutcome.
    srrace::RaceEndContext BuildRaceEndContext() const;
    bool PlayerBusted() const { return sim.PlayerBusted(); }
    int SpeedingFine() const { return sim.SpeedingFine(); }
    // Time is in tenths of a second; distance is the player's share of the opponent's, capped at 99.
    // Cruising: the race renderer with the race off, driving itself to the arch. signAsset is drawn on
    // approach segments flagged 0x0800.
    void StartCruise(int startSeg, int signAsset);
    bool CruiseFinished() const;
    int CruiseArrivalSide() const;

    int RaceTimeTenths() const;
    // How far behind the opponent finished, in tenths. 0 when the player was
    // first or when either car never crossed the line.
    int LossMarginTenths() const;
    int AverageSpeedMph() const;
    int DistancePercent() const;
    // In the original's 0..10000 units.
    int PlayerEngineWearDelta() const { return sim.Player().engineWearDelta; }
    int PlayerTransWearDelta() const { return sim.Player().transWearDelta; }
    int32_t PlayerSegment() const { return sim.PlayerSegment(); }
    bool PlayerTiresBlown() const { return sim.PlayerTiresBlown(); }
    bool IsAutomatic() const;
    // An automatic shows N/D, not the gear.
    int GetNumGears() const;
    bool IsAutoDriveEngaged() const;

    bool ShouldShowCrashOverlay() const;
    bool ShouldShowCrashBlood() const;

    RaceOutcome GetRaceOutcome() const;
    void ResetRaceOutcome();
    void ResetForNewRace();
    // A cruise has no outcome to stop the loop, so leaving the scene must.
    void StopEngineSound();

    void SetDebugFeaturesEnabled(bool enabled);
    bool IsFlybyMode() const { return flybyMode; }
    bool ToggleFlybyMode();
    bool OnFlybyKeyDown(SDL_Keycode key);
    std::vector<std::string> GetFlybyDebugLines(int lastVisibleRow) const;
    std::vector<DebugRow> GetRacingDebugRows(int lastVisibleRow) const;
    std::vector<DebugRow> GetOpponentDebugRows() const;
    std::vector<DebugIndicator> GetOpponentIndicators() const;
    bool* DebugInvulnerableFlag() { return &debugInvulnerable; }
    int GetLastVisibleRow() const { return lastVisibleRow; }
    bool SaveRoadViewPng(std::string* savedPath = nullptr);
};
