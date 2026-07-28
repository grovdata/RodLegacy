#pragma once

#include "global.h"
#include "ui/forward_declarations.h"
#include "mouse.h"
#include "parsers/libp.h"
#include "bitmap.h"
#include "carbitmap.h"
#include "debug_overlay.h"
#include "audio/audio_manager.h"
#include "utils.h"
#include <string>
#include <vector>
#include <memory>

typedef enum {
    SCENE_BLACK,
    SCENE_SOLID,
    SCENE_FADE_OUT,
    SCENE_FADE_IN
} SceneTransition;

class Car;

class Scene: public IDrawableContainer {
   private:

    Zone* FindZone(int x, int y);
    void UpdateFade(Uint64 deltaTime);


   protected:
    bool isSetup;

    SceneTransition sceneTransition;
    SrLibGameScene sceneTransitioningTo;

    Asset backgroundAsset;
    StockPalette palette;
    Bitmap *background;
    std::unique_ptr<HUD> hud;
    Dialog *activeDialog = nullptr;  // Non-owning pointer - dialogs owned by derived classes

    sdl2::SurfacePtr surface;
    sdl2::TexturePtr texture;
    sdl2::TexturePtr sceneTarget;
    SDL_Rect m_backgroundRect;
    SrLibAmigaSpriteAsset currentPointer = SrLibAmigaSpriteAssetMousePointerHand;

    std::string backgroundName;
    std::vector<Zone> zones;
    std::vector<Button> buttons;
    std::vector<SDL_Texture*> textures;
    std::vector<std::unique_ptr<Bubble>> bubbles;

    DebugOverlay* debugOverlay;
    bool debugMode;
    bool debugEnabled;
    
    Uint8 fadeAlpha = 255;
    Uint32 lastActionTimestamp = 0;  // SDL event timestamp for debouncing
    Uint64 fadeStartTime = 0;
    static constexpr Uint64 FADE_DURATION_MS = 500; // 500ms fade duration
    
    virtual void ToggleDebugMode();
    virtual void DrawDebugZones();

   public:
    Scene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    Scene(std::string backgroundName);
    Scene(); // No background constructor
    ~Scene();
    virtual void Init(bool debugMode);

    SDL_Texture *GetTexture();

    virtual SrLibAmigaSpriteAsset GetPointerType() { return currentPointer; }

    virtual void FadeOut();
    void FadeIn();
    void PrepareFadeIn();
    virtual void OnFadeComplete();
    virtual void Update(Uint64 deltaTime); // Update fade and other per-frame logic
    virtual void OnSceneExit();

    // Whether the custom mouse pointer should show in this scene. Applied
    // centrally on scene entry (FadeIn); override to false to hide it.
    virtual bool WantsPointer() const { return true; }

    void Draw();                // Non-virtual: manages render target + fade color mod
    virtual void DrawScene();  // Override in derived classes for scene-specific drawing
    virtual void DrawExtra() {} // Override in derived classes for additional drawing
    virtual bool PushEvent(SDL_Event *event);

    virtual Dialog *GetActiveDialog();
    virtual void SetActiveDialog(Dialog *dialog);

    void SetZoneText(const std::string& id, const std::string& text);
    void SetZoneRect(const std::string& id, SDL_Rect rect);
    void SetZoneEnabled(const std::string& id, bool enabled);
    Zone& GetZone(const std::string& id);
    const std::vector<Zone>& GetZones() const { return zones; }
    virtual void DrawDebugUI() {}

    virtual void UpdateDebugInfo();

    // Debug methods
    void AddGamepadDebugInfo(const Mouse& mouse);
};

enum class CarAnimationType {
    OPPONENT_CONTINUOUS,  // For Diner: Cars drive in, wait, drive out, repeat
    SINGLE_CAR_DRIVE,    // Car driving in to idle position and stopping
    SINGLE_CAR_REVERSE   // Car reversing out of screen
};

class CarScene: public Scene {
private:
    // Original game: 2 pixels/frame at 50 Hz PAL = 100 px/s constant linear speed
    static constexpr double CAR_SPEED_PX_PER_SEC = 100.0;
    double carxFloat = 0.0;  // sub-pixel accumulator
    double carAnimationTime = 0;
    double carExitAnimationTime = 0;
    double waitTimer = 0;
    double engineStartTimer = 0;  // Timer for engine start sample duration
    bool exitingCar = false;
    bool waitingForEngineStart = false;  // True when playing engine start sample
    int16_t lastAnimationEnginePeriod = -1;  // Avoid continuous period rewrites
    const double WAIT_BEFORE_EXIT = 3.0;
    const double WAIT_BEFORE_NEXT = 2.0;
    const double ENGINE_START_DURATION = CalculateSampleDuration(SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    bool isPaused = false;

protected:
    AudioManager* audioManager = nullptr;
    CarAnimationType currentAnimationType = CarAnimationType::SINGLE_CAR_DRIVE;
    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double initStartTime;
    double lastFrameTime;
    double currentTime;

    virtual void UpdateCarAnimation(double deltaTime);
    virtual void OnCarAnimationComplete() {};
    virtual void OnCarExitComplete() {}

    // Return false to hold the opponent in place (e.g. while a dialog is open).
    // The wait timer is reset while this returns false so the car stays the
    // full idle period after the block is lifted.
    virtual bool CanStartCarExit() const { return true; }

    // Immediately start the car exit animation (bypasses idle wait timer).
    void TriggerCarExit();

    // Helper to get Amiga period for car animations
    int16_t GetCarAnimationPeriod(double progress, bool isDrivingIn);

    bool needsNewAnimation = true;

public:
    CarScene(std::string backgroundName);
    CarScene(Asset backgroundAsset);
    virtual void Init(bool debugMode);
    virtual void DrawMaskAfterCar();
    virtual void DrawExtra() override; // Call DrawMaskAfterCar for car scenes
    virtual void StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType);
    virtual SDL_Point GetCarIdlePosition();
    virtual void UpdateDebugInfo() override;
    virtual bool PushEvent(SDL_Event *event) override;
    virtual void OnFadeComplete() override;
    virtual void OnSceneExit() override;
    
    void SetAudioManager(AudioManager* audioMgr) { audioManager = audioMgr; }

    CarBitmap *carBitmap = nullptr;
    bool animatingCar = false;
    int carx, cary;
};
