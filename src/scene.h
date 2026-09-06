#pragma once

#include "global.h"
#include "ui/forward_declarations.h"
#include "mouse.h"
#include "parsers/libp.h"
#include "bitmap.h"
#include "carbitmap.h"
#include "debug_overlay.h"
#include "debug_ui.h"
#include "audio/audio_manager.h"
#include "utils.h"
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

struct DrawableEntry {
    std::string id;
    IDrawable* drawable;  // Non-owning; ownership is elsewhere (e.g. AssetManager or scene)
};

typedef enum {
    SCENE_BLACK,
    SCENE_SOLID,
    SCENE_FADE_OUT,
    SCENE_FADE_IN
} SceneTransition;

class Car;

class Scene {
   private:

    Zone* FindZone(int x, int y);
    void UpdateFade(Uint64 deltaTime);


   protected:
    std::vector<DrawableEntry> drawables;

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
    // Debug text collected by UpdateDebugInfo() for this frame, rebuilt each
    // time it is asked for. Prefer AddDebugValue() for anything that reads as
    // "key: value"; AddDebugLine() is for free-form text.
    std::vector<std::string> debugLines;
    std::vector<DebugRow> debugRows;
    
    // Amiga fade state: fadeLevel is the 4-bit ceiling every colour channel is
    // clamped to, 15 = untouched, 0 = black. One step per PAL frame.
    // See RodLegacyResearch/docs/palette_fade.md.
    Uint8 fadeLevel = 0;
    Uint32 lastActionTimestamp = 0;  // SDL event timestamp for debouncing
    Uint64 fadeStartTime = 0;
    static constexpr Uint64 FADE_STEP_MS = 35;    // 20ms = one PAL frame, adjusted slightly to look better
    static constexpr Uint8 FADE_LEVELS = 15;      // top 4-bit colour level
    static constexpr Uint64 FADE_IN_STEPS = 16;   // 320ms, as the Amiga
    static constexpr Uint64 FADE_OUT_STEPS = 15;  // 300ms, as the Amiga

    void DrawFadeClamp();
    
    virtual void ToggleDebugMode();
    virtual void DrawDebugZones();
    // Draws the debug text into the game view. Only does anything in builds
    // without the ImGui debug UI, which otherwise owns the text.
    void DrawLegacyDebugOverlay();

   public:
    Scene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    Scene(std::string backgroundName);
    Scene(); // No background constructor
    ~Scene();
    virtual void Init(bool debugMode);

    SDL_Texture *GetTexture();

    virtual SrLibAmigaSpriteAsset GetPointerType() { return currentPointer; }

    // Sets currentPointer and applies it to the actual cursor right away.
    // currentPointer alone is only picked up on scene entry (FadeIn) - a
    // scene that wants the pointer to change on click (e.g. gas station's
    // pump vs. rag) needs this instead of assigning currentPointer directly.
    void SetPointer(SrLibAmigaSpriteAsset type);

    virtual void FadeOut();
    void FadeIn();
    virtual void OnFadeComplete();
    virtual void Update(Uint64 deltaTime); // Update fade and other per-frame logic
    // Called on entry, before the fade-in starts and so before the scene is
    // drawn once. Per-entry state belongs here, not in OnFadeComplete: that
    // runs 560ms later, and everything drawn until then is the previous
    // visit's leftovers fading up.
    virtual void OnSceneEnter();
    virtual void OnSceneExit();

    // Whether the custom mouse pointer should show in this scene. Applied
    // centrally on scene entry (FadeIn); override to false to hide it.
    virtual bool WantsPointer() const { return true; }

    void Draw();                // Non-virtual: manages render target + fade clamp
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

    // Fills debugLines. Override to add scene specific information; call the
    // base first to keep the common lines.
    virtual void UpdateDebugInfo();
    void AddDebugLine(const std::string& text) { debugLines.push_back(text); }
    void AddDebugValue(const std::string& key, const std::string& value) {
        debugRows.push_back(DebugRow{key, value});
    }

    // Inspector text for the debug UI: runs UpdateDebugInfo() and returns the
    // result as one panel titled after the scene. Override to supply several
    // panels instead.
    virtual std::vector<DebugPanel> GetDebugPanels();

    // Debug methods
    void AddGamepadDebugInfo(const Mouse& mouse);

    void AddDrawable(const std::string& id, IDrawable* drawable) {
        drawables.push_back({id, drawable});
    }

    // Swaps what an id points at without moving it in the list, so draw order
    // survives. Adds it at the end if the id is new.
    void SetDrawable(const std::string& id, IDrawable* drawable) {
        for (auto& entry : drawables) {
            if (entry.id == id) {
                entry.drawable = drawable;
                return;
            }
        }
        drawables.push_back({id, drawable});
    }

    void RemoveDrawable(const std::string& id) {
        drawables.erase(
            std::remove_if(drawables.begin(), drawables.end(),
                [&id](const DrawableEntry& entry) { return entry.id == id; }),
            drawables.end()
        );
    }

    void ClearDrawables() {
        drawables.clear();
    }

    IDrawable* GetDrawable(const std::string& id) {
        for (const auto& entry : drawables) {
            if (entry.id == id) {
                return entry.drawable;
            }
        }
        return nullptr;
    }

    void DrawAll() {
        for (const auto& entry : drawables) {
            if (entry.drawable) {
                entry.drawable->Draw();
            }
        }
    }

    void SetAllVisible(bool visible) {
        for (const auto& entry : drawables) {
            if (entry.drawable) {
                entry.drawable->SetVisible(visible);
            }
        }
    }

    size_t GetDrawableCount() const {
        return drawables.size();
    }
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
    CarScene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    void Init(bool debugMode) override;
    virtual void DrawMaskAfterCar();
    virtual void DrawExtra() override; // Call DrawMaskAfterCar for car scenes
    virtual void StartAnimateCar(CarPtr car, Driver *driver, CarAnimationType animType);
    virtual SDL_Point GetCarIdlePosition();
    virtual void UpdateDebugInfo() override;
    virtual bool PushEvent(SDL_Event *event) override;
    virtual void OnFadeComplete() override;
    virtual void OnSceneExit() override;
    
    void SetAudioManager(AudioManager* audioMgr) { audioManager = audioMgr; }

    // Whether a zone click should be handled right now: no scene fade and no
    // car drive-in/out animation in progress. A subclass with more ways to
    // be mid-action (e.g. Garage's tire-change animation) should shadow this
    // with its own CanInteract() that also checks CarScene::CanInteract().
    bool CanInteract() const { return sceneTransition == SCENE_SOLID && !animatingCar; }

    CarBitmap *carBitmap = nullptr;
    bool animatingCar = false;
    int carx, cary;
};
