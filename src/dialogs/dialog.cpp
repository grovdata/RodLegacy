#include "dialog.h"

Dialog::Dialog(const DialogConfig& config) : mainBitmap(nullptr), config(config) {
    mainBitmap = new Bitmap(GAME_WIDTH, GAME_HEIGHT, 32);
    mainBitmap->SetPosition(0, 0);

    extraBitmaps = map<string, Bitmap*>();

    mainBitmap->SetVisible(true);
    
    for (const auto& contentArea : config.contentAreas) {
        AddContentArea(contentArea, config.backgroundColor, config.borderColor);
    }
    
    for (const auto& buttonCfg : config.buttons) {   
        AddButton(buttonCfg);
    }

    for (const auto& lineCfg : config.lines) {
        AddLine(lineCfg, config.borderColor);
    }

    for (auto text : config.texts) {
        int x = text.rect.x;
        if (text.centered) {
            int textWidth = SFont_TextWidth(config.font, text.text.c_str());
            x = text.rect.x + (text.rect.w - textWidth) / 2;
        }
        
        SFont_Write(mainBitmap->GetSurface(), config.font, x, text.rect.y, text.text.c_str());
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), text.color.r, text.color.g, text.color.b);
        mainBitmap->UpdateFromSurface();
    }
    
    // Copy background image if provided
    if (config.useBackgroundImage && config.backgroundBitmap != nullptr) {
        // Create a new bitmap and copy the contents from the source bitmap
        backgroundBitmap = new Bitmap();
        *backgroundBitmap = config.backgroundBitmap;
        
        // Set the position and dimensions to match the dialog bounds
        backgroundBitmap->SetPosition(config.bounds.x, config.bounds.y);
        backgroundBitmap->SetDimensions(config.bounds.w, config.bounds.h);
        backgroundBitmap->SetVisible(true);
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
    if (backgroundBitmap) {
        delete backgroundBitmap;
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
    config.onClick = [this, buttonConfig]() {
        this->OnButtonPressed(buttonConfig.index);
    };
    buttons.push_back(new Button(config));
}

void Dialog::AddText(const char* text, const SDL_Rect& area) {
    SFont_Write(mainBitmap->GetSurface(), config.font, area.x, area.y, text);
}

void Dialog::AddBitmap(Bitmap *bitmap, const string& bitmapName, const SDL_Rect& area) {
    bitmap->SetPosition(area.x, area.y);
    bitmap->SetVisible(true);
    extraBitmaps[bitmapName] = bitmap;
}

void Dialog::Draw() {
    if (!isVisible) return;
    
    // Draw background image first if available
    if (backgroundBitmap && backgroundBitmap->IsVisible()) {
        backgroundBitmap->Draw();
    }
    
    for (auto rect : contentAreas) {
        rect->Draw();
    }
    
    for (auto button : buttons) {
        button->Draw();
    }

    for (auto bitmap : extraBitmaps) {
        bitmap.second->Draw();
    }

    mainBitmap->Draw();
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
    if (backgroundBitmap) {
        backgroundBitmap->SetVisible(true);
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
    if (backgroundBitmap) {
        backgroundBitmap->SetVisible(false);
    }
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