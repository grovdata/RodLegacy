#ifndef MOUSE_H
#define MOUSE_H


#include "global.h"
#include "bitmap.h"

extern SDL_Renderer *renderer;


class Mouse {
private:
    SDL_Texture* cursorTexture;
    SDL_Rect rect;
    SrLibAmigaSpriteAsset pointerType = SrLibAmigaSpriteAssetMousePointerHand;
    std::map<SrLibAmigaSpriteAsset, SDL_Texture*> pointerTextures;
     
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
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create cursor texture: %s", SDL_GetError());
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

    void Update(SDL_Event *event) {
        if (event->type == SDL_MOUSEMOTION) {
            rect.x = event->motion.x;
            rect.y = event->motion.y;
        }

        if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT) {
            switch (pointerType) {
                case SrLibAmigaSpriteAssetMousePointerHand:
                    pointerType = SrLibAmigaSpriteAssetMousePointerWrench;
                    break;
                case SrLibAmigaSpriteAssetMousePointerWrench:
                    pointerType = SrLibAmigaSpriteAssetMousePointerPump;
                    break;
                case SrLibAmigaSpriteAssetMousePointerPump:
                    pointerType = SrLibAmigaSpriteAssetMousePointerRag;
                    break;
                case SrLibAmigaSpriteAssetMousePointerRag:
                    pointerType = SrLibAmigaSpriteAssetMousePointerThumbsUp;
                        break;
                case SrLibAmigaSpriteAssetMousePointerThumbsUp:
                    pointerType = SrLibAmigaSpriteAssetMousePointerThumbsDown;
                    break;
                case SrLibAmigaSpriteAssetMousePointerThumbsDown:
                    pointerType = SrLibAmigaSpriteAssetMousePointerHand;
                    break;
            }
            cursorTexture = pointerTextures[pointerType];
        }
    }

    void Draw() {
        SDL_RenderCopy(renderer, cursorTexture, NULL, &rect);
    }

    void SetPointerType(SrLibAmigaSpriteAsset type) {
        if (pointerTextures.find(type) != pointerTextures.end()) {
            SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[type];
            rect.w = info.width;
            rect.h = info.height;
            pointerType = type;
            cursorTexture = pointerTextures[type];
        }
    }
};

#endif