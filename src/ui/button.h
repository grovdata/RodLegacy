#pragma once

#include <functional>
#include "../bitmap.h"
#include "../global.h"
#include "zone.h"
#include "rectangle.h"

static const SDL_Color ButtonDefaultTextColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultFillColor = {144, 144, 144, 255};
static const SDL_Color ButtonDefaultBorderColor = {0, 0, 0, 255};
static const SDL_Color ButtonDefaultChoiceColor = {255, 255, 255, 255};

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

    // Optional bitmap configuration
    struct {
        Bitmap* bitmap = nullptr;
        SDL_Rect sourceRect = {0, 0, 0, 0};
    } bitmapConfig;
};


class Button: public IDrawable {
private:
    ButtonConfig config;
    std::unique_ptr<Rectangle> background;
    std::unique_ptr<Bitmap> textBitmap;
    bool pressed;
    bool visible;

    void CreateButtonSurface();
    void CropBitmapFromSource();

public:
    Button(const ButtonConfig& config);
    ~Button() = default;

    void Draw() override;
    void HandleEvent(SDL_Event* event);
    void SetVisible(bool visible) override;
    bool IsVisible() const;
    bool IsDefaultChoice() const { return config.defaultChoice; }
    void SimulateClick();
};
