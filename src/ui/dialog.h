#pragma once
#include "../global.h"
#include "../bitmap.h"
#include "../SFont.h"
#include "button.h"
#include <vector>
#include <string>
#include <memory>

class Dialog {
protected:
    SDL_Rect bounds;
    std::string title;
    bool visible;
    const SFont_Font* font;
    std::vector<Button*> buttons;
    
    // Optional list functionality
    bool hasListBox;
    std::vector<std::string> items;
    int selectedIndex;
    int scrollOffset;
    int visibleItems;
    Button* scrollUpBtn;
    Button* scrollDownBtn;
    
    SDL_Surface* surface;
    SDL_Texture* texture;
    
public:
    Dialog(const std::string& title, const SDL_Rect& bounds, const SFont_Font* font);
    virtual ~Dialog();
    
    void Show();
    void Hide();
    void Draw();
    void HandleEvent(SDL_Event* event);
    
    void AddButton(Button* button);
    void EnableList();
    void AddItem(const std::string& item);
    void SetItems(const std::vector<std::string>& items);
    bool IsVisible() const { return visible; }
    int GetSelectedIndex() const { return selectedIndex; }
}; 