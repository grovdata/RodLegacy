#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/audio_manager.h"
#include "bitmap.h"
#include "parsers/carcolor.h"
#include "parsers/libp.h"
#include "race/race_sim.h"
#include "race/race_types.h"
#include "ui/hud.h"

class Car;

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
    bool automaticOverride = false;
    bool simInitialized = false;
    bool bShowCrashOverlay = false;
    bool crashHandled = false;
    Uint32 crashOverlayEndMs = 0;
    float fixedAccumulator = 0.0f;
    bool prevSpaceDown = false;
    bool pendingShift = false;
    int firstPlayerFinishTick = -1;
    int firstNpcFinishTick = -1;
    int simTickCounter = 0;

    srrace::TrackData track {};
    srrace::RaceRng rng;
    int trackVariant = 0;
    srrace::RaceSim sim;
    std::array<RowProjection, 15> rows {};

    float cloudOffsetAccum = 0.0f;
    // Currently shown NPC rear LOD frame; changed with hysteresis so it does not
    // flip back and forth when the longitudinal gap hovers on a frame boundary.
    int npcRearFrame = 0;
    // Sprite-space row where the mountain silhouette meets the foothill green
    // band it is drawn on. Cached on first use; this row is what sits on the
    // horizon, not the bitmap's lowest opaque row. -1 = uncomputed.
    int mountainBaseRow = -1;

    std::array<std::unique_ptr<Bitmap>, 20> roadsideSprites {};
    // Per-slot animation frames (pre-drawn scale levels) when the sprite has them;
    // nullptr slots fall back to kSpriteScale nearest-neighbor scaling.
    std::array<const SpriteAnimationInfo*, 20> roadsideAnim {};
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

    bool opponentActive = false;
    int opponentCarType = 0;
    StandardCarColor opponentColor = CarColorRed;
    RaceOutcome raceOutcome = RaceOutcome::Ongoing;

    bool debugFeaturesEnabled = false;
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
    std::unique_ptr<Bitmap> LoadOpponentSprite(uint16_t spriteId);
    void LoadOpponentCarSprites();
    void InitializeRaceSimulation();
    srrace::CarSetup BuildCarSetupFromCar(const Car* car, bool isPlayer) const;
    int RaceTypeFromTrackType(SrTrackType trackType) const;
    int ProjectX(const srrace::CarState& player, int d, int roadDelta, int lateral) const;
    int BuildRowTableAndGetLastVisible();
    void RenderSkyAndParallax(float frameSeconds);
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
    bool IsTransmissionDamaged() const;
    bool IsAutomatic() const;

    bool ShouldShowCrashOverlay() const;
    void HideCrashOverlay();

    RaceOutcome GetRaceOutcome() const;
    void ResetRaceOutcome();
    void ResetForNewRace();

    void SetDebugFeaturesEnabled(bool enabled);
    bool IsFlybyMode() const { return flybyMode; }
    bool ToggleFlybyMode();
    bool OnFlybyKeyDown(SDL_Keycode key);
    std::vector<std::string> GetFlybyDebugLines(int lastVisibleRow) const;
    int GetLastVisibleRow() const { return lastVisibleRow; }
    bool SaveRoadViewPng(std::string* savedPath = nullptr);
};
