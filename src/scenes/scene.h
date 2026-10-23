/*
 * RodLegacy - Scene base classes: Scene (background, fades, zones, dialogs) and CarScene (car drive-in,
 * drive-out and engine sound).
 *
 * Created: 2023-02-26
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "ui/forward_declarations.h"
#include "core/mouse.h"
#include "parsers/libp.h"
#include "gfx/bitmap.h"
#include "car/carbitmap.h"
#include "debug/debug_overlay.h"
#include "debug/debug_ui.h"
#include "audio/audio_manager.h"
#include "core/utils.h"
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

struct DrawableEntry {
    std::string id;
    IDrawable* drawable;
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

    SceneTransition sceneTransition = SCENE_SOLID;
    SrLibGameScene sceneTransitioningTo;

    Asset backgroundAsset;
    StockPalette palette;
    Bitmap *background;
    std::unique_ptr<HUD> hud;
    Dialog *activeDialog = nullptr;

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
    bool debugEnabled = false;
    // Rebuilt each time it is asked for.
    std::vector<std::string> debugLines;
    std::vector<DebugRow> debugRows;
    
    // fadeLevel is the 4-bit ceiling every channel is clamped to, 15 = untouched. One step per PAL frame.
    Uint8 fadeLevel = 0;
    Uint32 lastActionTimestamp = 0;
    Uint64 fadeStartTime = 0;
    static constexpr Uint64 FADE_STEP_MS = 35;    // 20ms = one PAL frame, adjusted slightly to look better
    static constexpr Uint8 FADE_LEVELS = 15;
    static constexpr Uint64 FADE_IN_STEPS = 16;   // 320ms, as the Amiga
    static constexpr Uint64 FADE_OUT_STEPS = 15;  // 300ms, as the Amiga

    void DrawFadeClamp();
    
    virtual void ToggleDebugMode();
    virtual void DrawDebugZones();
    // Only without the ImGui debug UI.
    void DrawLegacyDebugOverlay();

   public:
    Scene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    Scene(std::string backgroundName);
    Scene();
    virtual ~Scene();
    virtual void Init(bool debugMode);

    SDL_Texture *GetTexture();

    virtual SrLibAmigaSpriteAsset GetPointerType() { return currentPointer; }

    // Applies the pointer now; currentPointer alone is only picked up on scene entry.
    void SetPointer(SrLibAmigaSpriteAsset type);

    virtual void FadeOut();
    void FadeIn();
    virtual void OnFadeComplete();
    virtual void Update(Uint64 deltaTime);
    // Before the fade-in. Per-entry state belongs here: OnFadeComplete runs 560ms later.
    virtual void OnSceneEnter();
    virtual void OnSceneExit();

    virtual bool WantsPointer() const { return true; }

    void Draw();
    virtual void DrawScene();
    virtual void DrawExtra() {}
    virtual bool PushEvent(SDL_Event *event);

    // Rects the gamepad D-pad can snap to; zones by default.
    virtual std::vector<SDL_Rect> NavTargets() const;
    virtual Dialog *GetActiveDialog();
    virtual void SetActiveDialog(Dialog *dialog);

    void SetZoneText(const std::string& id, const std::string& text);
    void SetZoneRect(const std::string& id, SDL_Rect rect);
    void SetZoneEnabled(const std::string& id, bool enabled);
    Zone& GetZone(const std::string& id);
    Zone* ZoneById(const std::string& id);
    const std::vector<Zone>& GetZones() const { return zones; }
    virtual void DrawDebugUI() {}

    virtual void UpdateDebugInfo();
    void AddDebugLine(const std::string& text) { debugLines.push_back(text); }
    void AddDebugValue(const std::string& key, const std::string& value) {
        debugRows.push_back(DebugRow{key, value});
    }

    virtual std::vector<DebugPanel> GetDebugPanels();

    void AddGamepadDebugInfo(const Mouse& mouse);

    void AddDrawable(const std::string& id, IDrawable* drawable) {
        drawables.push_back({id, drawable});
    }

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
    // 2 pixels/frame at 50 Hz PAL.
    static constexpr double CAR_SPEED_PX_PER_SEC = 100.0;
    double carAnimationTime = 0;
    double carExitAnimationTime = 0;
    double waitTimer = 0;
    double engineStartTimer = 0;
    bool exitingCar = false;
    bool waitingForEngineStart = false;
    const double WAIT_BEFORE_EXIT = 3.0;
    const double WAIT_BEFORE_NEXT = 2.0;
    const double ENGINE_START_DURATION = CalculateSampleDuration(SRL0_ENGINE_START_LENGTH, SRL0_ENGINE_START_RATE);
    bool isPaused = false;

protected:
    // A subclass overriding StartAnimateCar without delegating must sync this, or the next animation may be skipped.
    double carxFloat = 0.0;
    // The original steps whole 2/4/8 px per video frame. StepCarX keeps its average speed on its pixel grid, or
    // the wheel frame strobes at 60+ fps.
    static constexpr double SCENE_STEP_HZ = 35.0;
    double stepAccumulator = 0.0;
    double StepCarX(double deltaTime, double pxPerSecond);

    AudioManager* audioManager = nullptr;
    CarAnimationType currentAnimationType = CarAnimationType::SINGLE_CAR_DRIVE;
    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double initStartTime = 0.0;
    double lastFrameTime = 0.0;
    double currentTime = 0.0;

    virtual void UpdateCarAnimation(double deltaTime);
    virtual void OnCarAnimationComplete() {};
    virtual void OnCarExitComplete() {}

    // The wait timer resets while this is false, so the car idles its full period afterwards.
    virtual bool CanStartCarExit() const { return true; }

    void TriggerCarExit();

    // EngineSound::AnimationNote, sent when it changes.
    void ApplyAnimationEngine(bool drivingIn, bool secondPhase);
    int engineBase = 600;   // 600 + rand % 10 per animation
    int16_t lastAnimationEnginePeriod = -1;

    bool needsNewAnimation = true;

public:
    CarScene(std::string backgroundName);
    CarScene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    void Init(bool debugMode) override;
    virtual void DrawMaskAfterCar();
    virtual void DrawExtra() override;
    virtual void StartAnimateCar(CarPtr car, Driver *driver, CarAnimationType animType);
    virtual SDL_Point GetCarIdlePosition();
    virtual void UpdateDebugInfo() override;
    virtual bool PushEvent(SDL_Event *event) override;
    virtual void OnFadeComplete() override;
    virtual void OnSceneExit() override;
    
    void SetAudioManager(AudioManager* audioMgr) { audioManager = audioMgr; }

    // Subclasses with more ways to be busy shadow this and also check CarScene::CanInteract().
    bool CanInteract() const { return sceneTransition == SCENE_SOLID && !animatingCar; }

    CarBitmap *carBitmap = nullptr;
    bool animatingCar = false;
    int carx = 0, cary = 0;
};
