#ifndef MOUSE_H
#define MOUSE_H


#include "core/log.h"
#include "core/global.h"
#include "core/gamepad.h"
#include "gfx/bitmap.h"
#include "ui/zone.h"
#include "debug/debug_ui.h"
#include <climits>
#include <vector>

extern SDL_Renderer *renderer;

class Scene; // Forward declaration

class Mouse {
private:
    SDL_Texture* cursorTexture = nullptr;
    SDL_Rect rect = {0, 0, 0, 0};
    SrLibAmigaSpriteAsset pointerType = SrLibAmigaSpriteAssetMousePointerHand;
    std::map<SrLibAmigaSpriteAsset, SDL_Texture*> pointerTextures;
    
    // When false, the custom cursor is not drawn (e.g. in the driving scene).
    bool visible = true;

    // Gamepad support. The pad itself lives in the global Gamepad; this only
    // holds what the cursor needs to remember between frames.
    bool gamepadActive = false;
    Uint32 lastDPadTime = 0;
    static const Uint32 DPAD_REPEAT_DELAY = 200; // ms between D-pad repeats

    // D-pad navigation
    enum class DPadDirection {
        NONE,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    DPadDirection lastDPadDirection = DPadDirection::NONE;

    // Sub-pixel carry for analog cursor movement: at 320x200 a slow stick would
    // otherwise truncate to zero pixels every frame and never move.
    float subPixelX = 0.0f;
    float subPixelY = 0.0f;

    // Gamepad debug info
    struct GamepadDebugInfo {
        bool connected = false;
        float leftX = 0.0f;
        float leftY = 0.0f;
        bool buttonA = false;
        bool buttonB = false;
        bool buttonX = false;
        bool buttonY = false;
        bool buttonLB = false;
        bool buttonRB = false;
        std::string lastAction = "";
    };
    GamepadDebugInfo gamepadDebug;

    // What the D-pad can jump between: the active dialog's buttons when one is
    // up, otherwise the scene's enabled zones.
    std::vector<SDL_Rect> NavTargets(Scene* scene) const;
    void MoveCursorTo(const SDL_Rect& target);

    // Jump to closest zone in specified direction
    void JumpToClosestZoneInDirection(Scene* scene, DPadDirection direction);

    // Synthesise input at the cursor and hand it to the scene.
    void SendClick(Scene* scene, Uint8 button);
    void SendKey(Scene* scene, Uint32 type, SDL_Keycode key);
    // Hover (and so the HUD) is driven by motion events, which the pad never
    // produces on its own - see UpdateGamepad.
    void SendMotion(Scene* scene);

    // Last position a motion event was sent for, so one is sent only when the
    // cursor actually moves. INT_MIN so the first update always reports.
    int lastMotionX = INT_MIN;
    int lastMotionY = INT_MIN;
    
public:
    Mouse() {
        SDL_ShowCursor(false);
    }

    ~Mouse() {
        unloadAllPointers();
    }

    void loadPointers(const std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>>& mousePointers) {
        for (const auto& [type, bitmap] : mousePointers) {
            if (!bitmap) continue;

            SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[type];
            rect.w = info.width;
            rect.h = info.height;

            pointerTextures[type] = SDL_CreateTextureFromSurface(renderer, bitmap->GetSurface());
            if (!pointerTextures[type]) {
                LOG_ERROR("Failed to create cursor texture: %s", SDL_GetError());
            }
            if (type == this->pointerType) {
                cursorTexture = pointerTextures[type];
            }
        }
    }

    void unloadAllPointers() {
        for (auto& [type, texture] : pointerTextures) {
            if (texture) {
                SDL_DestroyTexture(texture);
            }
        }
        pointerTextures.clear();
        cursorTexture = nullptr;
    }

    void Update(SDL_Event *event, Scene* scene = nullptr);
    void UpdateGamepad(Scene* scene, Uint64 deltaMs);

    void Draw() {
        if (!visible) return;
        SDL_RenderCopy(renderer, cursorTexture, NULL, &rect);
    }

    void SetVisible(bool v) { visible = v; }
    bool IsVisible() const { return visible; }

    void SetPointerType(SrLibAmigaSpriteAsset type) {
        if (pointerTextures.find(type) != pointerTextures.end()) {
            SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[type];
            rect.w = info.width;
            rect.h = info.height;
            pointerType = type;
            cursorTexture = pointerTextures[type];
        }
    }
    
    // Gamepad methods
    void SetGamepadActive(bool active) { gamepadActive = active; }
    bool IsGamepadActive() const { return gamepadActive; }
    
    // Jump to closest zone (for D-pad navigation)
    void JumpToClosestZone(Scene* scene);
    
    // Debug methods
    const GamepadDebugInfo& GetGamepadDebugInfo() const { return gamepadDebug; }
    // Key/value form is the source of truth; the line form is derived from it
    // for the plain-text overlay.
    std::vector<DebugRow> GetGamepadDebugRows() const;
    std::vector<std::string> GetGamepadDebugLines() const;
};

#endif