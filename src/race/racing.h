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
#include "race/race_types.h"
#include "ui/hud.h"

class Car;

// Door mirror glass per row, screen coordinates. The glass in RACING_INTERIOR_1 is a trapezoid, wider on its
// upper rows. The source starts at 243: DrawRearviewMirror never paints column 242.
struct SideMirrorRow { int16_t left, right; };
inline constexpr int kSideMirrorTop  = 101;   // screen y of the first row
inline constexpr int kSideMirrorSrcX = 243;   // roof mirror glass, screen coords
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

    float cloudOffsetAccum = 0.0f;
    // The row that sits on the horizon, not the bitmap's lowest opaque row. -1 = uncomputed.
    int mountainBaseRow = -1;

    std::array<std::unique_ptr<Bitmap>, 20> roadsideSprites {};
    std::array<const SpriteAnimationInfo*, 20> roadsideAnim {};
    std::unique_ptr<Bitmap> mountainSprite;
    std::unique_ptr<Bitmap> cloudSprite;
    std::unique_ptr<Bitmap> startLightSprite;
    // Held full during staging, then retracts by 4/frame at launch.
    int startLightHeight = 32;
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

    void PutPixel(SDL_Surface* s, int x, int y, SDL_Color c);
    void DrawGaugeNeedle(SDL_Renderer* renderer, int originX, int originY, int gaugeValue);
    void LoadSpriteBitmaps();
    void DrawTownWalls(int lastVisible);
    void DrawRearviewMirror();
    void DrawSideMirror();
    std::unique_ptr<Bitmap> LoadOpponentSprite(uint16_t spriteId);
    void LoadOpponentCarSprites();
    void InitializeRaceSimulation();
    srrace::CarSetup BuildCarSetupFromCar(const Car* car, bool isPlayer) const;
    int ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const;
    int BuildRowTableAndGetLastVisible();
    void UpdateRenderView();
    void RenderSkyAndParallax(float frameSeconds);
    void RenderClouds(int horizonY);
    void RenderMountains(int horizonY);
    void DrawStartLight();
    void RenderRoad(int lastVisible);
    // 1:1 blit into the road surface, clipped. The source rect defaults to the whole bitmap.
    void DrawSprite(Bitmap* bitmap, int dstX, int dstY, bool anchorRight, bool anchorBottom,
                    int srcX0 = 0, int srcY0 = 0, int srcW = -1, int srcH = -1);
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
