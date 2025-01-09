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

struct DialogTextConfig {
    SDL_Rect rect;
    string text;
    SDL_Color color;
    bool centered = false;
};

struct DialogConfig {
    SFont_Font *font;
    SDL_Rect bounds;
    SDL_Color backgroundColor = {0x60, 0x60, 0x60, 0xFF};
    SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    vector<ButtonConfig> buttons;
    vector<DialogTextConfig> texts;
    vector<SDL_Rect> contentAreas;
    function<void(int)> onButtonPressed;
};

class Dialog {
protected:
    Bitmap *mainBitmap;
    vector<Button*> buttons;
    vector<Rectangle*> contentAreas;
    DialogConfig config;
    bool isVisible = false;
public:
    Dialog(const DialogConfig& config);
    virtual ~Dialog();
    
    void AddContentArea(const SDL_Rect& area, SDL_Color fillColor = {0x60, 0x60, 0x60, 0xFF}, SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF});
    void AddButton(const ButtonConfig& buttonCfg);
    void AddText(const char* text, const SDL_Rect& area);

    virtual void HandleEvent(SDL_Event *event) = 0;
    virtual void Draw() = 0;
    virtual void Show();
    virtual void Hide();
    virtual bool IsVisible() const { return isVisible; }
    void OnButtonPressed(int index);
}; 