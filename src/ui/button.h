#pragma once

#include <functional>
#include <memory>
#include "../global.h"
#include "../ui/forward_declarations.h"
#include "button_config.h"
#include <string>


class Button: public IDrawable {
private:
    ButtonConfig config;
    std::unique_ptr<Rectangle> background;
    std::unique_ptr<Rectangle> pressedFace;  // Inverted overlay for just the button face when pressed
    std::unique_ptr<Bitmap> textBitmap;
    bool pressed;
    bool visible;

    void CreateButtonSurface();
    void CropBitmapFromSource();

public:
    Button(const ButtonConfig& config);
    ~Button();

    void Draw() override;
    void HandleEvent(SDL_Event* event);
    void SetVisible(bool visible) override;
    bool IsVisible() const;
    bool IsDefaultChoice() const { return config.defaultChoice; }
    bool IsCancelChoice() const { return config.cancelChoice; }
    bool MatchesHotkey(SDL_Keycode key) const { return config.hotkey != SDLK_UNKNOWN && config.hotkey == key; }
    void SimulateClick();
    bool IsPointInside(int x, int y) const;
};
