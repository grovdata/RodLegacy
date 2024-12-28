#pragma once
#include "dialog.h"

class Button : public DialogComponent {
private:
    std::string text;
    std::function<void()> onClick;
    bool pressed;
    
public:
    Button(const SDL_Rect& bounds, const std::string& text, std::function<void()> onClick);
    void Draw() override;
    void HandleEvent(SDL_Event* event) override;
};

class ListBox : public DialogComponent {
private:
    std::vector<std::string> items;
    int selectedIndex;
    int scrollOffset;
    int visibleItems;
    Button scrollUpBtn;
    Button scrollDownBtn;
    
public:
    ListBox(const SDL_Rect& bounds);
    void Draw() override;
    void HandleEvent(SDL_Event* event) override;
    
    void AddItem(const std::string& item);
    void SetItems(const std::vector<std::string>& items);
    int GetSelectedIndex() const { return selectedIndex; }
    void SetSelectedIndex(int index);
}; 