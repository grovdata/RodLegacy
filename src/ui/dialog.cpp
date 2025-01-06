#include "dialog.h"

Dialog::Dialog(const SDL_Rect& bounds) {    
    mainBounds = bounds;
}

Dialog::~Dialog() {
    for (auto rect : contentAreas) {
        delete rect;
    }
    for (auto button : buttons) {
        delete button;
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

void Dialog::AddButton(Button* button) {
    buttons.push_back(button);
}

void Dialog::Draw() {
    if (!isVisible) return;
    
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