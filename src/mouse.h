#ifndef MOUSE_H
#define MOUSE_H


#include "log.h"
#include "global.h"
#include "bitmap.h"
#include "ui/zone.h"
#include <vector>

extern SDL_Renderer *renderer;

class Scene; // Forward declaration

class Mouse {
private:
    SDL_Texture* cursorTexture;
    SDL_Rect rect;
    SrLibAmigaSpriteAsset pointerType = SrLibAmigaSpriteAssetMousePointerHand;
    std::map<SrLibAmigaSpriteAsset, SDL_Texture*> pointerTextures;
    
    // When false, the custom cursor is not drawn (e.g. in the driving scene).
    bool visible = true;

    // Gamepad support
    bool gamepadActive = false;
    bool dPadPressed = false;
    Uint32 lastDPadTime = 0;
    const Uint32 DPAD_REPEAT_DELAY = 200; // ms between D-pad repeats
    
    // D-pad navigation
    enum class DPadDirection {
        NONE,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };
    DPadDirection lastDPadDirection = DPadDirection::NONE;
    
    // Gamepad debug info
    struct GamepadDebugInfo {
        bool connected = false;
        Sint16 leftX = 0;
        Sint16 leftY = 0;
        Uint8 hatState = 0;
        bool buttonA = false;
        bool buttonB = false;
        bool buttonX = false;
        bool buttonY = false;
        bool buttonLB = false;
        bool buttonRB = false;
        std::string lastAction = "";
    };
    GamepadDebugInfo gamepadDebug;
    
    // Find closest zone to current mouse position, returns nullptr if none
    const Zone* FindClosestZone(const std::vector<Zone>& zones, int mouseX, int mouseY);
    
    // Calculate distance between two points
    float CalculateDistance(int x1, int y1, int x2, int y2);
    
    // Jump to closest zone in specified direction
    void JumpToClosestZoneInDirection(Scene* scene, DPadDirection direction);
    
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
    void UpdateGamepad(Scene* scene);

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
    std::vector<std::string> GetGamepadDebugLines() const;
};

#endif