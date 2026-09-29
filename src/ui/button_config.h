#pragma once

#include <functional>
#include <string>
#include <SDL.h>
#include "core/global.h"
#include "gfx/SFont.h"

class Bitmap;

static const SDL_Color ButtonDefaultTextColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultFillColor = {144, 144, 144, 255};
static const SDL_Color ButtonDefaultBorderColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultChoiceColor = {255, 255, 255, 255};
// The original does not highlight pressed buttons; this grey is RodLegacy's.
static const SDL_Color ButtonPressedFillColor = {0xb1, 0xb1, 0xb1, 255};

struct ButtonConfig {
    SDL_Rect bounds;
    std::string text;
    std::function<void()> onClick;
    bool defaultChoice = false;
    int index = -1;
    SDL_Color textColor = ButtonDefaultTextColor;
    SDL_Color fillColor = ButtonDefaultFillColor;
    SDL_Color borderColor = ButtonDefaultBorderColor;
    bool invisible = false;

    struct {
        Bitmap* bitmap = nullptr;
        SDL_Rect sourceRect = {0, 0, 0, 0};
    } bitmapConfig = {};

    SFont_Font* font = nullptr;

    // Kept last so positional aggregate initialisation up to `font` still compiles.
    bool cancelChoice = false;          // Escape triggers this button, like defaultChoice does for Enter
    SDL_Keycode hotkey = SDLK_UNKNOWN;  // any other key that should also trigger this button
};