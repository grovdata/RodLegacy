#include "button.h"
#include "../state.h"

Button::Button(const ButtonConfig& config) 
    : config(config), pressed(false), visible(true)
{
    CreateButtonSurface();
}

void Button::CreateButtonSurface() {
    // Create background rectangle
    RectangleConfig rectConfig;
    rectConfig.rect = config.bounds;
    rectConfig.fillColor = pressed ? SDL_Color{255, 255, 255, 255} : config.fillColor;
    rectConfig.borderColor = config.defaultChoice ? SDL_Color{255, 255, 255, 255} : config.borderColor;
    rectConfig.inset = 1;
    
    background = std::make_unique<Rectangle>(rectConfig);
    background->SetVisible(true);

    // Create text bitmap
    textBitmap = std::make_unique<Bitmap>(config.bounds.w, config.bounds.h, 32);
    textBitmap->SetPosition(config.bounds.x, config.bounds.y);
    textBitmap->SetVisible(true);
    
    // Center text
    int textWidth = SFont_TextWidth(state.assets.GetMainFont(), config.text.c_str());
    int textHeight = SFont_TextHeight(state.assets.GetMainFont());
    int textX = (config.bounds.w - textWidth) / 2;
    int textY = (config.bounds.h - textHeight) / 2;

    SFont_Write(textBitmap->GetSurface(), state.assets.GetMainFont(), textX, textY, config.text.c_str());
    textBitmap->UpdateFromSurface();
    SDL_SetTextureColorMod(textBitmap->GetTexture(), config.textColor.r, config.textColor.g, config.textColor.b);
}

void Button::Draw() {
    if (!visible) return;
    background->Draw();
    textBitmap->Draw();
}

void Button::HandleEvent(SDL_Event* event) {
    if (!visible || !config.onClick) return;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                int x = event->button.x;
                int y = event->button.y;
                if (x >= config.bounds.x && x < config.bounds.x + config.bounds.w &&
                    y >= config.bounds.y && y < config.bounds.y + config.bounds.h) {
                    pressed = true;
                    CreateButtonSurface(); // Update appearance
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && pressed) {
                pressed = false;
                CreateButtonSurface(); // Update appearance
                
                int x = event->button.x;
                int y = event->button.y;
                if (x >= config.bounds.x && x < config.bounds.x + config.bounds.w &&
                    y >= config.bounds.y && y < config.bounds.y + config.bounds.h) {
                    config.onClick();
                }
            }
            break;
    }
}

void Button::SetVisible(bool visible) {
    this->visible = visible;
}

bool Button::IsVisible() const {
    return visible;
}

void Button::SimulateClick() {
    if (!visible || !config.onClick) return;
    
    pressed = true;
    CreateButtonSurface();
    config.onClick();
    pressed = false;
    CreateButtonSurface();
}