/*
 * RodLegacy - Gus's gas station scene: GasStation with the pump, the attendant and refuelling.
 *
 * Created: 2023-02-26
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "scenes/scene.h"
#include "ui/zone.h"
#include "core/utils.h"
#include "gfx/animated_bitmap.h"
#include "gfx/bitmap.h"
#include "ui/bubble.h"

enum GasStationState {
    GAS_STATION_IDLE,
    GAS_STATION_HANDLEOFF,
    GAS_STATION_PUMPING
};

class GasStation : public CarScene {
   private:
    sdl2::SurfacePtr maskedBackground;
    sdl2::TexturePtr maskedBackgroundTexture;
    // Built with asset 338 over the pump so the strip does not repaint the handle every frame.
    sdl2::SurfacePtr maskedBackgroundNoHandle;
    sdl2::TexturePtr maskedBackgroundNoHandleTexture;
    sdl2::SurfacePtr roadMaskSurface;
    sdl2::TexturePtr roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 175, 320, 15}; // Mask from y=175 down, 15 pixels high
    SDL_Rect roadMaskDestRect = {0, 0, 320, 15};
    // Street Rod 2: each pump's hose strip is the background with or without the hose and nozzle patched on.
    // Taking a nozzle swaps its strip to the bare one; the mask and the crop strip are cut again from the result.
    sdl2::SurfacePtr nozzleStrip[2][2];   // [pump][0 = patched, 1 = bare]
    bool nozzleOut[2] = {false, false};
    int activePump = -1;
    Bitmap* sr2PumpMask = nullptr;
    void SetNozzleOut(int pump, bool out);
    void RebuildSr2Overlays();

    GasStationState sceneState = GAS_STATION_IDLE;
    std::unique_ptr<CarBitmap> currentCarBitmap;
    std::unique_ptr<AnimatedBitmap> attendantAnim;
    // Drawn in DrawMaskAfterCar, not the drawables container, which DrawExtra would paint over.
    Bitmap* pumpFilling = nullptr;
    // The pump is a sandwich around the car: 338 behind it, the masked background on top. 339 is baked into
    // maskedBackgroundNoHandle; 306 is unused, being identical to the background.
    Bitmap* pumpNoHandle = nullptr;
    Bitmap* pumpSideMask = nullptr;         // asset 339
    static constexpr SDL_Point PUMP_POS = {56, 114};
    static constexpr SDL_Point PUMP_SIZE = {24, 62};
    // Owned by `bubbles`; kept here to trigger without indexing into it.
    Bubble* attendantBubble = nullptr;
    // Reused for each scold/status line; Bubble self-hides 5s after SetVisible(true).
    Bubble* pumpMessageBubble = nullptr;
    CarBitmap* GetOrCreateCurrentCarBitmap();

    // One pump loop iteration is 3 * 6 = 18 PAL frames.
    static constexpr double LOOP_ITERATION_SEC = 18.0 / 50.0;      // 0.36s
    static constexpr double IDLE_TIMEOUT_SEC = 40 * LOOP_ITERATION_SEC;  // 0x28 iterations
    static constexpr int ITERATIONS_PER_PUMP_TICK = 4;             // fuel/charge every 4th
    static constexpr int FUEL_PER_TICK_TENTHS = 30;                // 0x1e = 3.0 gal
    // ponytail: the original targets 19.0 gal while pumping and clamps to 17.0 on leaving, a grace the
    // player cannot see. Targeting the cap directly; change if exact tick counts ever matter.
    static constexpr int TANK_CAP_TENTHS = 170;                    // 0xaa = 17.0 gal

    // Street Rod 2: the pump picked sets the delivery, the tank holds 200, a loiterer waits 15 iterations.
    static constexpr int SR2_TANK_CAP = 200;
    static constexpr int SR2_PREMIUM_PER_TICK = 40;
    static constexpr int SR2_REGULAR_PER_TICK = 30;
    static constexpr double SR2_IDLE_TIMEOUT_SEC = 15 * LOOP_ITERATION_SEC;
    int sr2PerTick = SR2_PREMIUM_PER_TICK;   // the original starts on 0x28 and the jockey uses whatever is set
    bool TankFull(const Car* car) const;

    static constexpr double LEAVE_DELAY_SEC = 1.0;  // beat between handle back on and driving off

    double idleTimer = 0.0;
    double loopTimer = 0.0;
    double leaveTimer = -1.0;  // >= 0 once the fill is done and the car is about to leave
    int pricePerTick = 1;       // dollars; doubles each time the attendant takes over
    bool hasPumpedAnyFuel = false;
    bool waitingForAttendant = false;

    // A direct pixel fill, not a sprite. Both lines run off one counter: the right flips every iteration,
    // the left every 4th.
    static constexpr SDL_Rect PUMP_DISPLAY_FAST = {51, 123, 2, 1};
    static constexpr SDL_Rect PUMP_DISPLAY_SLOW = {48, 123, 2, 1};
    unsigned pumpDisplayCounter = 0;
    bool pumpDisplayFastLit = false;
    bool pumpDisplaySlowLit = false;

    void UpdatePumping(double deltaTime);
    // GasPumpDisplayBlink modes 1 and 3: start (counter cleared) and stop.
    void StartPumpDisplay();
    void StopPumpDisplay();
    // The pump's own chug, on for exactly as long as fuel is moving.
    bool pumpSoundOn = false;
    void SetPumpSound(bool on);
    void ShowPumpMessage(const std::string& text);

    // A growing then shrinking + sign, like the intro glint.
    static constexpr int TWINKLE_FRAMES = 8;
    static constexpr double TWINKLE_STEP_SEC = 0.06;
    bool ragHeld = false;
    double twinkleTime = -1.0;                       // >= 0 while playing
    SDL_Point twinklePos = {0, 0};
    void StartTwinkle(const char* windowZone);
    void DrawTwinkle(double deltaTime);

   public:
    GasStation();
    ~GasStation();
    void Init(bool initialDebugMode) override;

    SDL_Point GetCarIdlePosition() override;

    void DrawScene() override;
    void DrawMaskAfterCar() override;
    void UpdateCarAnimation(double deltaTime) override;
    void OnCarAnimationComplete() override;
    void OnCarExitComplete() override;
    void UpdateDebugInfo() override;
    bool PushEvent(SDL_Event* event) override;

    void StartAnimateCar(CarPtr car, Driver* driver, CarAnimationType animType) override;
    void OnFadeComplete() override;
    void FadeOut() override;
    void OnSceneExit() override;

};
