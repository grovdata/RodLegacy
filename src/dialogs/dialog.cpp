#include "dialogs/dialog.h"
#include "ui/rectangle.h"
#include "ui/button.h"
#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "ui/zone.h"
#include <vector>
#include <algorithm>

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
    ButtonConfig config = buttonConfig;
    
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

    std::vector<DialogBitmap> sortedBitmaps;
    for (const auto& bitmap : bitmaps) {
        sortedBitmaps.push_back(bitmap.second);
    }
    // Stable, so overlapping bitmaps on the same layer keep a consistent order.
    std::stable_sort(sortedBitmaps.begin(), sortedBitmaps.end(),
        [](const auto& a, const auto& b) { return a.zOrder < b.zOrder; });
    
    for (const auto& bitmap : sortedBitmaps) {
        if (auto* animBitmap = dynamic_cast<AnimatedBitmap*>(bitmap.bitmap)) {
            animBitmap->Update(1.0f/60.0f);
        }
    }
    
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

    // Bitmap-only dialogs skip mainBitmap; subclasses that draw into it set mainBitmapHasContent.
    bool hasContent = mainBitmapHasContent || !contentAreas.empty() ||
                      !config.texts.empty() || !config.lines.empty();
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

std::vector<SDL_Rect> Dialog::NavTargets() const {
    std::vector<SDL_Rect> out;
    for (const Button* b : buttons) {
        if (b && b->IsVisible()) out.push_back(b->GetBounds());
    }
    for (const Zone& z : config.zones) {
        if (z.IsEnabled()) out.push_back(z.GetRect());
    }
    return out;
}

Zone* Dialog::GetZone(const std::string& id) {
    for (auto& zone : config.zones) {
        if (zone.GetId() == id) return &zone;
    }
    return nullptr;
}

void ClipDialogBitmapToRect(DialogBitmap& db, const SDL_Rect& mask, SDL_Rect logicalRect) {
    SDL_Rect visible;
    if (!SDL_IntersectRect(&logicalRect, &mask, &visible)) {
        db.rect = {0, 0, 0, 0};
        db.sourceRect = {0, 0, 0, 0};
        return;
    }
    db.sourceRect = {visible.x - logicalRect.x, visible.y - logicalRect.y, visible.w, visible.h};
    db.rect = visible;
}

bool Dialog::HandleEvent(SDL_Event* event) {
    if (!IsVisible()) return false;

    for (auto button : buttons) {
        button->HandleEvent(event);
    }

    bool handled = false;

    // Always consumed while a dialog is up, matching Escape.
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
        handled = true;
        for (auto button : buttons) {
            if (button->IsDefaultChoice()) {
                button->SimulateClick();
                break;
            }
        }
    }

    // Always consumed, or an unhandled Escape quits the game via main.cpp. cancelChoice takes priority over onDismiss.
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        handled = true;
        Button* cancelButton = nullptr;
        for (auto button : buttons) {
            if (button->IsCancelChoice()) {
                cancelButton = button;
                break;
            }
        }
        if (cancelButton) cancelButton->SimulateClick();
        else if (config.onDismiss) config.onDismiss();
    }

    if (event->type == SDL_KEYDOWN) {
        for (auto button : buttons) {
            if (button->MatchesHotkey(event->key.keysym.sym)) {
                button->SimulateClick();
                handled = true;
                break;
            }
        }
    }

    if (config.onDismiss && event->type == SDL_MOUSEBUTTONDOWN) {
        const SDL_Rect& b = config.bounds;
        int x = event->button.x;
        int y = event->button.y;
        if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h) {
            config.onDismiss();
            return true;
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && config.onZoneClick && event->button.button == SDL_BUTTON_LEFT) {
        int x = event->button.x;
        int y = event->button.y;
        for (auto& zone : config.zones) {
            if (zone.IsInsideZone(x, y)) {
                config.onZoneClick(zone);
                return true;
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

    // Dialogs are mouse-modal: every mouse event is consumed.
    switch (event->type) {
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            handled = true;
            break;
        default:
            break;
    }

    return handled;
}