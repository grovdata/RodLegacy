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
    Rectangle* background;
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
    void SimulateClick();
    bool IsPointInside(int x, int y) const;
};
