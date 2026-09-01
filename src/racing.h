#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_manager.h"
#include "debug_ui.h"
#include "bitmap.h"
#include "parsers/carcolor.h"
#include "parsers/libp.h"
#include "race/race_end_message.h"
#include "race/race_sim.h"
#include "race/race_types.h"
#include "ui/hud.h"

class Car;

// Door mirror glass, one entry per row, screen coordinates, top row first.
//
// The original copies a flat 26x16 block of the roof mirror's glass from screen
// (242,27) into the interior bitmap at (7,1) = screen (7,101). The glass painted
// into RACING_INTERIOR_1 is wider than that on its upper rows - its right edge
// slants in three pixels down the 16 rows - so this covers the whole trapezoid
// instead of leaving a sliver of flat colour on the right.
//
// The source starts at 243, not the original's 242, which pulls the reflection
// one pixel left. DrawRearviewMirror insets its glass by a pixel (gLeft =
// mirrorX + 1), so column 242 is never painted and copying it laid a black
// stripe down the left of the door mirror.
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
    // Terms this race was started on, captured at init. The race-end message is
    // built from these rather than from State's pending bet, which by then is
    // settled and no longer describes the race that just ran.
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
    // Ticks left before the tire screech one-shot can retrigger. The original
    // (RaceLogicUpdate 0x244d50) refires it every tick the skid condition
    // holds, cutting the Paula channel short each time; our audio path only
    // queues whole samples, so instead we retrigger once the sample (~445 ms,
    // SRL0_TIRE_SCREECH_LENGTH/RATE) has had time to play out, giving a
    // continuous screech for as long as the slide lasts.
    int tireScreechCooldownTicks = 0;
    // Same retrigger scheme for the cop siren, sized to that sample (~886 ms =
    // SRL0_COP_SIREN_LENGTH/RATE, ~13 ticks at the 15 Hz sim rate). The original
    // restarts the Paula channel while the cop is closing; we requeue instead.
    int copSirenCooldownTicks = 0;

    srrace::TrackData track {};
    srrace::RaceRng rng;
    int trackVariant = 0;
    srrace::RaceSim sim;
    std::array<RowProjection, 15> rows {};

    float cloudOffsetAccum = 0.0f;
    // Currently shown NPC rear LOD frame. Moves by at most one step per sim tick
    // toward the distance-derived target (RenderNPCCar 0x237ea8).
    int npcRearFrame = 0;
    // Last sim tick on which npcRearFrame was allowed to step, so the LOD
    // advances at the sim rate (the original's frame rate) rather than the
    // render rate. -1 = not yet stepped.
    int npcRearFrameLastTick = -1;
    // Sprite-space row where the mountain silhouette meets the foothill green
    // band it is drawn on. Cached on first use; this row is what sits on the
    // horizon, not the bitmap's lowest opaque row. -1 = uncomputed.
    int mountainBaseRow = -1;

    std::array<std::unique_ptr<Bitmap>, 20> roadsideSprites {};
    // Per-slot animation frames (pre-drawn scale levels) when the sprite has them;
    // nullptr slots fall back to spriteScale nearest-neighbor scaling.
    std::array<const SpriteAnimationInfo*, 20> roadsideAnim {};
    // Per-slot, per-distance-row {w,h} read at load time from the Amiga
    // Street_Rod binary's roadside_sprite_table (see LoadSpriteScaleTable).
    std::array<std::array<std::array<uint8_t, 2>, 11>, 20> spriteScale {};
    std::unique_ptr<Bitmap> mountainSprite;
    std::unique_ptr<Bitmap> cloudSprite;
    std::unique_ptr<Bitmap> startLightSprite;
    // Start-light reveal height (0..32). Held full during the staging countdown,
    // then retracts by 4/frame as the car launches (original A4-0x13d0).
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
    // Opponent's real car type, stashed while the cop has the slot. -1 = no
    // swap in effect.
    int opponentCarTypeBeforeCop = -1;
    StandardCarColor opponentColor = CarColorRed;
    RaceOutcome raceOutcome = RaceOutcome::Ongoing;

    bool debugFeaturesEnabled = false;
    // Debug toggle: the checkbox in the Racing panel writes here and Draw()
    // pushes it into the sim each frame.
    bool debugInvulnerable = false;
    bool flybyMode = false;
    bool flybyNpcVisible = false;
    int32_t flybySeg = srrace::RACE_START_SEG;
    int16_t flybyScrollAcc = 0;
    int16_t flybyHeading = 0;
    int16_t flybyLateral = 2560;
    bool opponentActiveBeforeFlyby = false;
    int lastVisibleRow = -1;

    void PutPixel(SDL_Surface* s, int x, int y, SDL_Color c);
    void DrawGaugeNeedle(SDL_Renderer* renderer, int centerX, int centerY, float value, float minValue, float maxValue);
    void LoadSpriteBitmaps();
    void DrawRearviewMirror();
    void DrawSideMirror();
    std::unique_ptr<Bitmap> LoadOpponentSprite(uint16_t spriteId);
    void LoadOpponentCarSprites();
    void InitializeRaceSimulation();
    srrace::CarSetup BuildCarSetupFromCar(const Car* car, bool isPlayer) const;
    int RaceTypeFromTrackType(SrTrackType trackType) const;
    int ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const;
    int BuildRowTableAndGetLastVisible();
    void RenderSkyAndParallax(float frameSeconds);
    void RenderClouds(int horizonY);
    void RenderMountains(int horizonY);
    void DrawStartLight();
    void RenderRoad(int lastVisible);
    void DrawNearestNeighborScaled(Bitmap* bitmap, int dstX, int dstY, int dstW, int dstH, bool anchorRight, bool anchorBottom,
                                   int srcX0 = 0, int srcY0 = 0, int srcW = -1, int srcH = -1, bool flipX = false);
    void DrawRoadsideAndArchSprites(int lastVisible);
    void DrawOverheadArch(const RowProjection& row, uint16_t flags, int sr);
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

    void SetTransmissionType(bool automatic);

    void InitializeOpponent();
    void SetOpponentActive(bool active);
    void SetOpponentCarType(int carType);
    void SetOpponentColor(StandardCarColor color);

    float GetSpeed() const;

    int16_t GetCurrentGear() const;
    int16_t GetRPM() const;
    int16_t GetVelocity() const;
    // Eased steering position for the dashboard wheel: >0 right, <0 left, magnitude
    // up to ~24. Decays toward 0 over several frames after the stick is released
    // (the original's slide velocity A4+0x4b0e), so the wheel re-centers gradually.
    int16_t GetSteerDisplay() const;

    // Short human-readable reason the race ended (e.g. "CRASHED", "YOU WIN!").
    // Call while the outcome is still set (before ResetRaceOutcome).
    std::string GetRaceEndReason() const;
    // Pulled over for speeding, and what the ticket costs. See
    // RodLegacyResearch/docs/driving/cop.md.
    bool PlayerBusted() const { return sim.PlayerBusted(); }
    int SpeedingFine() const { return sim.SpeedingFine(); }
    bool IsTransmissionDamaged() const;
    bool IsAutomatic() const;

    bool ShouldShowCrashOverlay() const;
    bool ShouldShowCrashBlood() const;

    RaceOutcome GetRaceOutcome() const;
    void ResetRaceOutcome();
    void ResetForNewRace();

    void SetDebugFeaturesEnabled(bool enabled);
    bool IsFlybyMode() const { return flybyMode; }
    bool ToggleFlybyMode();
    bool OnFlybyKeyDown(SDL_Keycode key);
    // Two shapes of the same state, for two renderers: packed lines for the
    // narrow in-game overlay, one value per row for the debug UI's table.
    std::vector<std::string> GetFlybyDebugLines(int lastVisibleRow) const;
    std::vector<DebugRow> GetRacingDebugRows(int lastVisibleRow) const;
    // Opponent telemetry: what its AI decided this tick, and where it is.
    std::vector<DebugRow> GetOpponentDebugRows() const;
    std::vector<DebugIndicator> GetOpponentIndicators() const;
    bool* DebugInvulnerableFlag() { return &debugInvulnerable; }
    int GetLastVisibleRow() const { return lastVisibleRow; }
    bool SaveRoadViewPng(std::string* savedPath = nullptr);
};
