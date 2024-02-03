#ifndef MOUSE_H
#define MOUSE_H

#include "global.h"

extern SDL_Renderer *renderer;

enum MousePointer { PointerWrench = 0, PointerHand = 1, PointerFuelPump = 2, PointerRag = 3 };

class Mouse {
   private:
    SDL_Texture *cursorTexture;
    SDL_Rect rect;
    MousePointer pointerType = PointerHand;

    std::tuple<std::string, int, int> GetPointer() {
        std::tuple<std::string, int, int> result;
        switch (pointerType) {
            case PointerWrench:
                result = std::make_tuple("../data/pointers/pointer1.png", 16, 16);
                break;
            case PointerHand:
                result = std::make_tuple("../data/pointers/pointer2.png", 16, 12);
                break;
            case PointerFuelPump:
                result = std::make_tuple("../data/pointers/pointer3.png", 11, 16);
                break;
            case PointerRag:
                result = std::make_tuple("../data/pointers/pointer4.png", 16, 14);
                break;
        }
        return result;
    }

   public:
    Mouse() {
        load();
        SDL_ShowCursor(false);
    }

    ~Mouse() {
        unload();
    }

    void unload() {
        SDL_DestroyTexture(cursorTexture);
    }

    void load() {
        std::tuple<std::string, int, int> pointerMap = GetPointer();

        cursorTexture = IMG_LoadTexture(renderer, std::get<0>(pointerMap).c_str());
        if (cursorTexture == nullptr) {
            printf("Error loading mouse cursor\n");
        }
        rect.w = std::get<1>(pointerMap);
        rect.h = std::get<2>(pointerMap);
    }

    void Update(SDL_Event *event) {
        // Handle joystick input

        rect.x = event->motion.x;
        rect.y = event->motion.y;

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
            unload();
            load();
        }
    }

    void Draw() {
        SDL_RenderCopy(renderer, cursorTexture, NULL, &rect);
    }

    void SetPointerType(MousePointer type) {
        pointerType = type;
        unload();
        load();
    }
};

#endif