#pragma once

#include <functional>
#include "../bitmap.h"
#include "../global.h"
#include "zone.h"
#include "rectangle.h"

struct ButtonConfig {
    SDL_Rect bounds;
    std::string text;
    std::function<void()> onClick;
    bool defaultChoice;
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Color fillColor = {144, 144, 144, 255};
    SDL_Color borderColor = {0, 0, 0, 255};
};

class Button: public IDrawable {
private:
    ButtonConfig config;
    std::unique_ptr<Rectangle> background;
    std::unique_ptr<Bitmap> textBitmap;
    bool pressed;
    bool visible;

    void CreateButtonSurface();

public:
    Button(const ButtonConfig& config);
    ~Button() = default;

    void Draw() override;
    void HandleEvent(SDL_Event* event);
    void SetVisible(bool visible) { this->visible = visible; }
    bool IsVisible() const { return visible; }
};
