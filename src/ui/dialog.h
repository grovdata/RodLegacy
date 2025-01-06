#pragma once
#include "../global.h"
#include "../bitmap.h"
#include "../SFont.h"
#include "rectangle.h"
#include "button.h"
#include <vector>
#include <string>
#include <memory>

using namespace std;

struct DialogButtonConfig {
    string label;
    SDL_Rect rect;
    bool defaultButton = false;
    int index = -1;
};

struct DialogConfig {
    SDL_Rect bounds;
    vector<DialogButtonConfig> buttons;
    vector<SDL_Rect> contentAreas;
    function<void(int)> onButtonPressed;
};

class Dialog {
private:
    vector<Rectangle*> contentAreas;
    vector<Button*> buttons;
    bool isVisible = false;
protected:
    DialogConfig config;
public:
    Dialog(const DialogConfig& config);
    ~Dialog();
    
    void AddContentArea(const SDL_Rect& area, SDL_Color fillColor = {0x60, 0x60, 0x60, 0xFF}, SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF});
    void AddButton(Button* button);

    virtual void HandleEvent(SDL_Event *event) = 0;
    virtual void Draw() = 0;
    void Show();
    void Hide();
    bool IsVisible() const { return isVisible; }
}; 