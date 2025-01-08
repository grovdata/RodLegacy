#include "dialog.h"

Dialog::Dialog(const DialogConfig& config) {    
    this->config = config;
    mainBitmap = new Bitmap(config.bounds.w, config.bounds.h, 32);
    for (const auto& contentArea : config.contentAreas) {
        AddContentArea(contentArea);
    }
    
    for (const auto& buttonCfg : config.buttons) {
        
        AddButton(buttonCfg);
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

    mainBitmap->Draw();
    
    for (auto rect : contentAreas) {
        rect->Draw();
    }
    
    for (auto button : buttons) {
        button->Draw();
    }
}

void Dialog::Show() {
    isVisible = true;
    for (auto rect : contentAreas) {
        rect->SetVisible(true);
    }
    for (auto button : buttons) {
        button->SetVisible(true);
    }
}

void Dialog::Hide() {
    isVisible = false;
    for (auto rect : contentAreas) {
        rect->SetVisible(false);
    }
    for (auto button : buttons) {
        button->SetVisible(false);
    }
} 