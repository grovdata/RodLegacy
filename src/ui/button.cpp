#include "core/log.h"
#include "ui/button.h"
#include "ui/rectangle.h"
#include "core/state.h"
#include "gfx/bitmap.h"
#include "ui/zone.h"
#include "gfx/SFont.h"

Button::Button(const ButtonConfig& config)
    : config(config), pressed(false), visible(true)
{
    CreateButtonSurface();
}

Button::~Button() = default;

void Button::CropBitmapFromSource() {
    if (!config.bitmapConfig.bitmap || 
        config.bitmapConfig.sourceRect.w == 0 || 
        config.bitmapConfig.sourceRect.h == 0) {
        return;
    }

    SDL_Surface* croppedSurface = SDL_CreateRGBSurfaceWithFormat(
        0, config.bitmapConfig.sourceRect.w, config.bitmapConfig.sourceRect.h, 32, SDL_PIXELFORMAT_RGBA32);
    
    if (!croppedSurface) {
        LOG_ERROR("Failed to create cropped surface: %s", SDL_GetError());
        return;
    }

    SDL_Rect srcRect = config.bitmapConfig.sourceRect;
    SDL_BlitSurface(config.bitmapConfig.bitmap->GetSurface(), &srcRect, croppedSurface, nullptr);

    textBitmap = std::make_unique<Bitmap>();
    textBitmap->LoadSurface(croppedSurface);
    textBitmap->SetPosition(config.bounds.x, config.bounds.y);
    textBitmap->SetVisible(true);

    SDL_FreeSurface(croppedSurface);
}

void Button::CreateButtonSurface() {
    if (config.invisible) {
        return;
    }
    
    RectangleConfig rectConfig{};
    rectConfig.rect = config.bounds;
    rectConfig.fillColor = config.fillColor;
    rectConfig.borderColor = config.defaultChoice ? SDL_Color{255, 255, 255, 255} : config.borderColor;
    rectConfig.inset = 1;

    background = std::make_unique<Rectangle>(rectConfig);
    background->SetVisible(true);

    // A grey tint over the face only; the label keeps its colour.
    if (pressed) {
        RectangleConfig faceConfig{};
        faceConfig.rect = {config.bounds.x + 2, config.bounds.y + 2,
                            config.bounds.w - 4, config.bounds.h - 4};
        faceConfig.fillColor = ButtonPressedFillColor;
        faceConfig.borderColor = ButtonPressedFillColor;
        faceConfig.inset = 0;
        pressedFace = std::make_unique<Rectangle>(faceConfig);
        pressedFace->SetVisible(true);
    } else {
        pressedFace.reset();
    }

    if (config.bitmapConfig.bitmap) {
        if (config.bitmapConfig.sourceRect.w > 0 && config.bitmapConfig.sourceRect.h > 0) {
            CropBitmapFromSource();
        } else {
            textBitmap = std::make_unique<Bitmap>();
            *textBitmap = config.bitmapConfig.bitmap;
            textBitmap->SetPosition(config.bounds.x, config.bounds.y);
            textBitmap->SetVisible(true);
        }
    } else {
        textBitmap = std::make_unique<Bitmap>(config.bounds.w, config.bounds.h, 32);
        textBitmap->SetPosition(config.bounds.x, config.bounds.y);
        textBitmap->SetVisible(true);
        
        SFont_Font* font = config.font ? config.font : state.GetAssets().GetMainFont();
        int textWidth = SFont_TextWidth(font, config.text.c_str());
        int textHeight = SFont_TextHeight(font);
        int textX = (config.bounds.w - textWidth) / 2;
        int textY = (config.bounds.h - textHeight) / 2;

        SFont_Write(textBitmap->GetSurface(), font, textX, textY, config.text.c_str());
        textBitmap->UpdateFromSurface();
        SDL_SetTextureColorMod(textBitmap->GetTexture(),
                               config.textColor.r, config.textColor.g, config.textColor.b);
    }
}

void Button::Draw() {
    if (!visible || config.invisible) return;
    background->Draw();
    if (pressedFace) pressedFace->Draw();
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
                    if (!config.invisible) {
                        CreateButtonSurface();
                    }
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && pressed) {
                pressed = false;
                if (!config.invisible) {
                    CreateButtonSurface();
                }
                
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

bool Button::IsPointInside(int x, int y) const {
    return x >= config.bounds.x && x < config.bounds.x + config.bounds.w &&
           y >= config.bounds.y && y < config.bounds.y + config.bounds.h;
}