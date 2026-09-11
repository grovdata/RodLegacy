#pragma once

#include "global.h"
#include "scene.h"
#include "ui/zone.h"
#include "utils.h"
#include "animated_bitmap.h"
#include "bitmap.h"
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
    // Same strip, built from a background with asset 338 blitted over the pump
    // so the handle is gone. Without this the strip repaints the handle from
    // the original background over everything, every frame.
    sdl2::SurfacePtr maskedBackgroundNoHandle;
    sdl2::TexturePtr maskedBackgroundNoHandleTexture;
    sdl2::SurfacePtr roadMaskSurface;
    sdl2::TexturePtr roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 175, 320, 15}; // Mask from y=175 down, 15 pixels high
    SDL_Rect roadMaskDestRect = {0, 0, 320, 15};
    GasStationState sceneState = GAS_STATION_IDLE;
    std::unique_ptr<CarBitmap> currentCarBitmap;
    std::unique_ptr<AnimatedBitmap> attendantAnim;
    // Neither is in the drawables container: DrawAll runs inside DrawScene,
    // but DrawExtra (and so DrawMaskAfterCar, which repaints the pump over the
    // car) runs after it, so anything in the container at the pump's or the
    // car's position gets covered. Both are drawn manually in
    // DrawMaskAfterCar instead.
    Bitmap* pumpFilling = nullptr;
    // The pump is a sandwich around the car, both assets 24x62 at (56,114):
    // 338 (the background slice with the handle erased) behind the car, then
    // the car, then the masked background on top, which is what makes the pump
    // occlude the car. 339 is the pump's front edge without the handle and is
    // baked into maskedBackgroundNoHandle rather than drawn. Asset 306 (front
    // edge with the handle) is unused: it is identical to the background.
    Bitmap* pumpNoHandle = nullptr;         // asset 338
    Bitmap* pumpSideMask = nullptr;         // asset 339
    static constexpr SDL_Point PUMP_POS = {56, 114};
    static constexpr SDL_Point PUMP_SIZE = {24, 62};
    // Owned by the shared `bubbles` vector (see constructor); kept here too
    // so UpdatePumping can trigger it without indexing into that vector.
    Bubble* attendantBubble = nullptr;
    // Transient scold/status lines (messages 69/70/71 - see gas_station.md),
    // reused for whichever one fires; Bubble self-hides 5s after SetVisible(true).
    Bubble* pumpMessageBubble = nullptr;
    CarBitmap* GetOrCreateCurrentCarBitmap();

    // Pacing verified against the Amiga disassembly - see
    // RodLegacyResearch/docs/driving/gas_station.md. GasStationPumpLoop runs
    // one iteration per CarEventLoop(3) call, and CarEventLoop counts one unit
    // per 6 VBlank wakeups, so one iteration is 3 * 6 = 18 PAL frames.
    static constexpr double LOOP_ITERATION_SEC = 18.0 / 50.0;      // 0.36s
    static constexpr double IDLE_TIMEOUT_SEC = 40 * LOOP_ITERATION_SEC;  // 0x28 iterations
    static constexpr int ITERATIONS_PER_PUMP_TICK = 4;             // fuel/charge every 4th
    static constexpr int FUEL_PER_TICK_TENTHS = 30;                // 0x1e = 3.0 gal
    // ponytail: the original targets 190 (19.0gal) while pumping and clamps to
    // 170 (17.0gal) only on leaving, a 2.0gal grace invisible to the player.
    // Targeting the cap directly here instead; upgrade if a ticks-needed count
    // that exactly matches the original ever matters.
    static constexpr int TANK_CAP_TENTHS = 170;                    // 0xaa = 17.0 gal

    static constexpr double LEAVE_DELAY_SEC = 1.0;  // beat between handle back on and driving off

    double idleTimer = 0.0;
    double loopTimer = 0.0;
    double leaveTimer = -1.0;  // >= 0 once the fill is done and the car is about to leave
    int pricePerTick = 1;       // dollars; doubles each time the attendant takes over
    bool hasPumpedAnyFuel = false;
    // True from the moment the attendant takes over until his stand-up
    // animation finishes and the bubble line has been shown once.
    bool waitingForAttendant = false;

    // The pump's display, verified as a direct pixel fill in the Amiga binary
    // (GasPumpDisplayBlink, 0x24611e), not a sprite - it isn't in any bitmap
    // asset. Two 2x1 horizontal lines at a fixed screen position (the pump
    // doesn't move with the car), both driven off the same counter (A4+0x4cd0):
    // the right one flips every loop iteration, the left one every 4th.
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
