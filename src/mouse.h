#ifndef MOUSE_H
#define MOUSE_H

#include "global.h"

// TODO: Enum for all cursor types
// Hand
// Wrench
// Fuelpump
// Rag

enum MousePointer {
    PointerHand      = 0,
    PointerWrench    = 1,
    PointerFuelPump  = 2,
    PointerRag       = 3
};

class Mouse {
    private:
        SDL_Texture *cursorTexture;
        SDL_Rect rect;
        SDL_Rect logicalRect;
        MousePointer pointerType = PointerHand;
        
        std::tuple<string, int, int> getPointer() {
            std::tuple<string, int, int> result;
            switch (pointerType) {
                case PointerHand:
                    result = make_tuple("../data/pointer1.png", 16, 16);                    
                    break;
                case PointerWrench:
                    result = make_tuple("../data/pointer2.png", 16, 12);
                    break;
                case PointerFuelPump:
                    result = make_tuple("../data/pointer3.png", 11, 16);
                    break;
                case PointerRag:                
                    result = make_tuple("../data/pointer4.png", 16, 14);
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
            std::tuple<string, int, int> pointerMap = getPointer();
             
            cursorTexture = IMG_LoadTexture(renderer, get<0>(pointerMap).c_str());
            rect.w = get<1>(pointerMap);
            rect.h = get<2>(pointerMap);
            logicalRect.w = get<1>(pointerMap);
            logicalRect.h = get<2>(pointerMap);
        }        

        void update() {
            Uint32 buttonState = SDL_GetMouseState(&rect.x, &rect.y);
            if (buttonState & SDL_BUTTON(3)) {
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

            // We must convert coordinates into logical ones since gfx is scaled
            float logicalX, logicalY;
            SDL_RenderWindowToLogical(renderer, rect.x, rect.y, &logicalX, &logicalY);
            logicalRect.x = (int)logicalX;
            logicalRect.y = (int)logicalY;
        }

        void draw() {
            SDL_RenderCopy(renderer, cursorTexture, NULL, &logicalRect);
        }
};

#endif