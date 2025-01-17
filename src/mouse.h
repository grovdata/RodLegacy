#ifndef MOUSE_H
#define MOUSE_H


#include "global.h"
#include "bitmap.h"

extern SDL_Renderer *renderer;

enum MousePointer { 
    PointerWrench = 0,
    PointerHand = 1,
    PointerFuelPump = 2,
    PointerRag = 3
};

class Mouse {
private:
    SDL_Texture* cursorTexture;
    SDL_Rect rect;
    MousePointer pointerType = PointerHand;
    std::map<MousePointer, SDL_Texture*> pointerTextures;
    
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
            MousePointer pointerType = static_cast<MousePointer>(type);
            pointerTextures[pointerType] = SDL_CreateTextureFromSurface(renderer, bitmap->GetSurface());
            if (!pointerTextures[pointerType]) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create cursor texture: %s", SDL_GetError());
            }
            if (pointerType == this->pointerType) {
                cursorTexture = pointerTextures[pointerType];
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

    void Update(SDL_Event *event) {
        if (event->type == SDL_MOUSEMOTION) {
            rect.x = event->motion.x;
            rect.y = event->motion.y;
        }

        if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT) {
            switch (pointerType) {
                case PointerHand:
                    pointerType = PointerWrench;
                    break;
                case PointerWrench:
                    pointerType = PointerFuelPump;
                    break;
                case PointerFuelPump:
                    pointerType = PointerRag;
                    break;
                case PointerRag:
                    pointerType = PointerHand;
                    break;
            }
            cursorTexture = pointerTextures[pointerType];
        }
    }

    void Draw() {
        SDL_RenderCopy(renderer, cursorTexture, NULL, &rect);
    }

    void SetPointerType(MousePointer type) {
        if (pointerTextures.find(type) != pointerTextures.end()) {
            pointerType = type;
            cursorTexture = pointerTextures[type];
        }
    }
};

#endif