#include "dialog.h"
#include "../ui/rectangle.h"
#include "../ui/button.h"
#include "../bitmap.h"
#include "../animated_bitmap.h"
#include "../spritesheet.h"
#include "ui/zone.h"
#include <vector>

Dialog::Dialog(const DialogConfig& config) : mainBitmap(nullptr), config(config) {
    mainBitmap = new Bitmap(GAME_WIDTH, GAME_HEIGHT, 32);
    mainBitmap->SetPosition(0, 0);

    mainBitmap->SetVisible(true);
    
    for (const auto& contentArea : config.contentAreas) {
        AddContentArea(contentArea, config.backgroundColor, config.borderColor);
    }
    
    for (const auto& buttonCfg : config.buttons) {   
        AddButton(buttonCfg);
    }
    
    for (auto button : config.buttonPtrs) {
        buttons.push_back(button);
    }

    for (const auto& lineCfg : config.lines) {
        AddLine(lineCfg, config.borderColor);
    }

    for (const auto& text : config.texts) {
        int x = text.rect.x;
        if (text.centered) {
            int textWidth = SFont_TextWidth(config.font, text.text.c_str());
            x = text.rect.x + (text.rect.w - textWidth) / 2;
        }
        
        SFont_Write(mainBitmap->GetSurface(), config.font, x, text.rect.y, text.text.c_str());
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), text.color.r, text.color.g, text.color.b);
        mainBitmap->UpdateFromSurface();
    }
    bitmaps = config.bitmaps;
    for (const auto& bitmap : config.bitmaps) {
        bool useSourceRect = (bitmap.second.sourceRect.w > 0 && bitmap.second.sourceRect.h > 0);
        if (!useSourceRect) {
            bitmap.second.bitmap->SetPosition(bitmap.second.rect.x, bitmap.second.rect.y);
            bitmap.second.bitmap->SetDimensions(bitmap.second.rect.w, bitmap.second.rect.h);
        }
        bitmap.second.bitmap->SetVisible(true);
    }
}

Dialog::~Dialog() {
    delete mainBitmap;
    for (auto rect : contentAreas) {
        delete rect;
    }
    for (auto button : buttons) {
        delete button;
    }
}

void Dialog::OnButtonPressed(int index) {
    if (config.onButtonPressed) {
        config.onButtonPressed(index);
    }
}

void Dialog::AddContentArea(const SDL_Rect& area, SDL_Color fillColor, SDL_Color borderColor) {
    RectangleConfig config;
    config.rect = area;
    config.fillColor = fillColor;
    config.borderColor = borderColor;
    config.inset = 1;
    
    contentAreas.push_back(new Rectangle(config));
}

void Dialog::AddLine(SDL_Rect rect, SDL_Color color) {
    SDL_FillRect(mainBitmap->GetSurface(), &rect, SDL_MapRGB(mainBitmap->GetSurface()->format, color.r, color.g, color.b));
    mainBitmap->UpdateFromSurface();
}

void Dialog::AddButton(const ButtonConfig& buttonConfig) {
    ButtonConfig config = buttonConfig;  // Make a copy so we can modify it
    
    // Only override onClick if no custom callback was provided
    if (!config.onClick) {
        config.onClick = [this, buttonConfig]() {
            this->OnButtonPressed(buttonConfig.index);
        };
    }
    
    buttons.push_back(new Button(config));
}

void Dialog::AddText(const char* text, const SDL_Rect& area) {
    SFont_Write(mainBitmap->GetSurface(), config.font, area.x, area.y, text);
}

void Dialog::Draw() {
    if (!isVisible) return;

    for (auto rect : contentAreas) {
        rect->Draw();
    }

    // Sort in z-order
    std::vector<DialogBitmap> sortedBitmaps;
    for (const auto& bitmap : bitmaps) {
        sortedBitmaps.push_back(bitmap.second);
    }
    sort(sortedBitmaps.begin(), sortedBitmaps.end(),
        [](const auto& a, const auto& b) { return a.zOrder < b.zOrder; });
    
    // Update animations
    for (const auto& bitmap : sortedBitmaps) {
        if (auto* animBitmap = dynamic_cast<AnimatedBitmap*>(bitmap.bitmap)) {
            animBitmap->Update(1.0f/60.0f);  // Assume 60 FPS
        }
    }
    
    // Draw bitmaps; if sourceRect is set, blit only that sub-region of the
    // texture (works uniformly for Bitmap and Spritesheet, since Spritesheet
    // is-a Bitmap whose texture already holds the full sheet)
    for (const auto& bitmap : sortedBitmaps) {
        if (bitmap.sourceRect.w > 0 && bitmap.sourceRect.h > 0) {
            if (bitmap.bitmap->IsVisible()) {
                bitmap.bitmap->DrawWithRects(bitmap.sourceRect, bitmap.rect);
            }
        } else {
            bitmap.bitmap->SetPosition(bitmap.rect.x, bitmap.rect.y);
            bitmap.bitmap->SetDimensions(bitmap.rect.w, bitmap.rect.h);
            bitmap.bitmap->Draw();
        }
    }

    for (auto button : buttons) {
        button->Draw();
    }

    // Only draw mainBitmap when dialog has content (text/areas/lines); bitmap-only dialogs skip it
    bool hasContent = !contentAreas.empty() || !config.texts.empty() || !config.lines.empty();
    if (hasContent) {
        mainBitmap->Draw();
    }
}

void Dialog::Show() {
    isVisible = true;
    mainBitmap->SetVisible(true);
    for (auto rect : contentAreas) {
        rect->SetVisible(true);
    }
    for (auto button : buttons) {
        button->SetVisible(true);
    }
    for (auto bitmap : bitmaps) {
        bitmap.second.bitmap->SetVisible(true);
    }
}

void Dialog::Hide() {
    isVisible = false;
    mainBitmap->SetVisible(false);
    for (auto rect : contentAreas) {
        rect->SetVisible(false);
    }
    for (auto button : buttons) {
        button->SetVisible(false);
    }
    for (auto bitmap : bitmaps) {
        bitmap.second.bitmap->SetVisible(false);
    }
} 

DialogBitmap* Dialog::GetBitmap(const std::string& id) {
    auto it = bitmaps.find(id);
    if (it != bitmaps.end()) {
        return &(it->second);
    }
    return nullptr;
}

void Dialog::HandleEvent(SDL_Event* event) {
    if (!IsVisible()) return;

    // Handle button events
    for (auto button : buttons) {
        button->HandleEvent(event);
    }

    // Handle Enter key for default button
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
        // Find the default button
        for (auto button : buttons) {
            if (button->IsDefaultChoice()) {
                button->SimulateClick();
                break;
            }
        }
    }

    // Dismiss on Escape or click outside dialog bounds
    if (config.onDismiss) {
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            config.onDismiss();
            return;
        }
        if (event->type == SDL_MOUSEBUTTONDOWN) {
            const SDL_Rect& b = config.bounds;
            int x = event->button.x;
            int y = event->button.y;
            if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) {
                config.onDismiss();
                return;
            }
        }
    }

    // Zone click: inside dialog bounds, check which zone was hit
    if (event->type == SDL_MOUSEBUTTONDOWN && config.onZoneClick && event->button.button == SDL_BUTTON_LEFT) {
        int x = event->button.x;
        int y = event->button.y;
        for (auto& zone : config.zones) {
            if (zone.IsInsideZone(x, y)) {
                config.onZoneClick(zone);
                return;
            }
        }
    }

    if (event->type == SDL_MOUSEMOTION) {
        SDL_Point mousePos = {event->motion.x, event->motion.y};
        for (auto zone : config.zones) {
            if (zone.IsInsideZone(mousePos.x, mousePos.y)) {
                if (config.onZoneHover) {
                    config.onZoneHover(zone);
                }
                break;
            }
        }
    }

}