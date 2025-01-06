#pragma once
#include "../global.h"
#include "../bitmap.h"
#include "../SFont.h"
#include "rectangle.h"
#include "button.h"
#include <vector>
#include <string>
#include <memory>

class Dialog {
private:
    std::vector<Rectangle*> contentAreas;
    std::vector<Button*> buttons;
    bool isVisible = false;
protected:
    SDL_Rect mainBounds;
public:
    Dialog(const SDL_Rect& bounds);
    ~Dialog();
    
    void AddContentArea(const SDL_Rect& area, SDL_Color fillColor = {0x60, 0x60, 0x60, 0xFF}, SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF});
    void AddButton(Button* button);
    virtual void HandleEvent(SDL_Event *event) = 0;
    virtual void Draw() = 0;
    void Show();
    void Hide();
    bool IsVisible() const { return isVisible; }
}; 