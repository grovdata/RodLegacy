#include "button.h"
#include "rectangle.h"
#include "../state.h"
#include "../bitmap.h"
#include "zone.h"
#include "../SFont.h"

Button::Button(const ButtonConfig& config) 
    : config(config), pressed(false), visible(true), background(nullptr)
{
    CreateButtonSurface();
}

Button::~Button() {
    delete background;
}

void Button::CropBitmapFromSource() {
    if (!config.bitmapConfig.bitmap || 
        config.bitmapConfig.sourceRect.w == 0 || 
        config.bitmapConfig.sourceRect.h == 0) {
        return;
    }

    // Create a new surface for the cropped bitmap
    SDL_Surface* croppedSurface = SDL_CreateRGBSurfaceWithFormat(
        0, config.bitmapConfig.sourceRect.w, config.bitmapConfig.sourceRect.h, 32, SDL_PIXELFORMAT_RGBA32);
    
    if (!croppedSurface) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create cropped surface: %s", SDL_GetError());
        return;
    }

    // Copy the portion of the source bitmap
    SDL_Rect srcRect = config.bitmapConfig.sourceRect;
    SDL_BlitSurface(config.bitmapConfig.bitmap->GetSurface(), &srcRect, croppedSurface, nullptr);

    // Create a new bitmap from the cropped surface
    textBitmap = std::make_unique<Bitmap>();
    textBitmap->LoadSurface(croppedSurface);
    textBitmap->SetPosition(config.bounds.x, config.bounds.y);
    textBitmap->SetVisible(true);

    // Clean up
    SDL_FreeSurface(croppedSurface);
}

void Button::CreateButtonSurface() {
    // Create background rectangle
    RectangleConfig rectConfig{};
    rectConfig.rect = config.bounds;
    rectConfig.fillColor = pressed ? SDL_Color{255, 255, 255, 255} : config.fillColor;
    rectConfig.borderColor = config.defaultChoice ? SDL_Color{255, 255, 255, 255} : config.borderColor;
    rectConfig.inset = 1;
    
    background = new Rectangle(rectConfig);
    background->SetVisible(true);

    // Create text bitmap or use provided bitmap
    if (config.bitmapConfig.bitmap) {
        if (config.bitmapConfig.sourceRect.w > 0 && config.bitmapConfig.sourceRect.h > 0) {
            // Crop from source bitmap
            CropBitmapFromSource();
        } else {
            // Use the provided bitmap as is
            textBitmap = std::make_unique<Bitmap>();
            *textBitmap = config.bitmapConfig.bitmap;  // Copy the bitmap
            textBitmap->SetPosition(config.bounds.x, config.bounds.y);
            textBitmap->SetVisible(true);
        }
    } else {
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