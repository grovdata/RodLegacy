#pragma once
#include "../global.h"
#include "../bitmap.h"
#include "../SFont.h"
#include "ui/rectangle.h"
#include "ui/button.h"
#include "ui/zone.h"
#include <vector>
#include <map>

#include <string>
#include <memory>

using namespace std;

struct DialogTextConfig {
    SDL_Rect rect;
    string text;
    SDL_Color color;
    bool centered = false;
};

struct DialogBitmap {
    Bitmap* bitmap;
    SDL_Rect rect;
};

struct DialogConfig {
    SFont_Font *font;
    SDL_Rect bounds;
    SDL_Color backgroundColor = {0x60, 0x60, 0x60, 0xFF};
    SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    vector<ButtonConfig> buttons;
    vector<DialogTextConfig> texts;
    vector<SDL_Rect> contentAreas;
    vector<SDL_Rect> lines;
    vector<Zone> zones;
    
    DialogBitmap backgroundBitmap = {nullptr, {0, 0, 0, 0}};
    bool useBackgroundImage = false;
    map<string, DialogBitmap> extraBitmaps;
    
    function<void(int)> onButtonPressed;
    function<void(Zone&)> onZoneHover;
};

class Dialog {
protected:
    Bitmap *mainBitmap;
    map<string, Bitmap*> extraBitmaps;
    vector<Button*> buttons;
    vector<Rectangle*> contentAreas;
    DialogConfig config;
    bool isVisible = false;
    
    // Background image
    Bitmap* backgroundBitmap = nullptr;
    
public:
    Dialog(const DialogConfig& cfg);
    virtual ~Dialog();
    
    void AddContentArea(const SDL_Rect& area, SDL_Color fillColor = {0x60, 0x60, 0x60, 0xFF}, SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF});
    void AddButton(const ButtonConfig& buttonCfg);
    void AddText(const char* text, const SDL_Rect& area);
    void AddLine(SDL_Rect rect, SDL_Color color);

    virtual void HandleEvent(SDL_Event *event);
    virtual void Draw();
    virtual void Show();
    virtual void Hide();
    virtual bool IsVisible() const { return isVisible; }
    void OnButtonPressed(int index);
}; 