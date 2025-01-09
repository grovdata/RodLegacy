#include "dialog.h"

Dialog::Dialog(const DialogConfig& config) {    
    this->config = config;
    mainBitmap = new Bitmap(config.bounds.w, config.bounds.h, 32);
    mainBitmap->SetPosition(config.bounds.x, config.bounds.y);
    mainBitmap->SetVisible(true);
    for (const auto& contentArea : config.contentAreas) {
        AddContentArea(contentArea);
    }
    
    for (const auto& buttonCfg : config.buttons) {   
        AddButton(buttonCfg);
    }

     for (auto text : config.texts) {
        // Clear rect before
        SFont_Write(mainBitmap->GetSurface(), config.font, text.rect.x, text.rect.y, text.text.c_str());
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), text.color.r, text.color.g, text.color.b);
        mainBitmap->UpdateFromSurface();
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

void Dialog::AddButton(DialogButtonConfig buttonCfg) {
    Button* button = new Button(buttonCfg.label, buttonCfg.rect, [this, buttonCfg]() {
        this->OnButtonPressed(buttonCfg.index);
    });
    buttons.push_back(button);
}

void Dialog::AddText(const char* text, const SDL_Rect& area) {
    SFont_Write(mainBitmap->GetSurface(), config.font, area.x, area.y, text);
}

void Dialog::Draw() {
    if (!isVisible) return;

    
    for (auto rect : contentAreas) {
        rect->Draw();
    }
    
    for (auto button : buttons) {
        button->Draw();
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
} 