/*
 * RodLegacy - The game's own mouse pointer: Mouse draws it and drives it from the mouse, D-pad and analog
 * stick.
 *
 * Created: 2023-02-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

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

class Scene;

class Mouse {
private:
    SDL_Texture* cursorTexture = nullptr;
    SDL_Rect rect = {0, 0, 0, 0};
    SrLibAmigaSpriteAsset pointerType = SrLibAmigaSpriteAssetMousePointerHand;
    std::map<SrLibAmigaSpriteAsset, SDL_Texture*> pointerTextures;
    
    bool visible = true;

    bool gamepadActive = false;
    Uint32 lastDPadTime = 0;
    static const Uint32 DPAD_REPEAT_DELAY = 200; // ms between D-pad repeats

    enum class DPadDirection {
        NONE,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    DPadDirection lastDPadDirection = DPadDirection::NONE;

    // A slow stick would otherwise truncate to zero pixels every frame.
    float subPixelX = 0.0f;
    float subPixelY = 0.0f;

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

    std::vector<SDL_Rect> NavTargets(Scene* scene) const;
    void MoveCursorTo(const SDL_Rect& target);

    void JumpToClosestZoneInDirection(Scene* scene, DPadDirection direction);

    void SendClick(Scene* scene, Uint8 button);
    void SendKey(Scene* scene, Uint32 type, SDL_Keycode key);
    // Hover is driven by motion events, which the pad never produces on its own.
    void SendMotion(Scene* scene);

    // INT_MIN so the first update always reports.
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
    
    void SetGamepadActive(bool active) { gamepadActive = active; }
    bool IsGamepadActive() const { return gamepadActive; }
    
    void JumpToClosestZone(Scene* scene);
    
    const GamepadDebugInfo& GetGamepadDebugInfo() const { return gamepadDebug; }
    std::vector<DebugRow> GetGamepadDebugRows() const;
    std::vector<std::string> GetGamepadDebugLines() const;
};

#endif