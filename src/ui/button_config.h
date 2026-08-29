#pragma once

#include <functional>
#include <string>
#include <SDL.h>
#include "../global.h"
#include "../SFont.h"

// Forward declaration
class Bitmap;

static const SDL_Color ButtonDefaultTextColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultFillColor = {144, 144, 144, 255};
static const SDL_Color ButtonDefaultBorderColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultChoiceColor = {255, 255, 255, 255};
// Clicked-state face fill. The original game does not highlight buttons at all;
// this is a light grey chosen for RodLegacy rather than an inversion.
static const SDL_Color ButtonPressedFillColor = {0xb1, 0xb1, 0xb1, 255};

// Text and bitmap are mutually exclusive
// If bitmap is provided, text will still be used for hover event
struct ButtonConfig {
    SDL_Rect bounds;
    std::string text;
    std::function<void()> onClick;
    bool defaultChoice = false;
    int index = -1;
    SDL_Color textColor = ButtonDefaultTextColor;
    SDL_Color fillColor = ButtonDefaultFillColor;
    SDL_Color borderColor = ButtonDefaultBorderColor;
    bool invisible = false;  // If true, button won't display any graphics but can still be clicked

    // Optional bitmap configuration
    struct {
        Bitmap* bitmap = nullptr;
        SDL_Rect sourceRect = {0, 0, 0, 0};
    } bitmapConfig = {};

    SFont_Font* font = nullptr;  // If null, falls back to state.GetAssets().GetMainFont()
};