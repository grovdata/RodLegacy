#pragma once
#include "../global.h"
#include "../SFont.h"
#include "../ui/zone.h"  // For ZoneId
#include "../ui/rectangle.h"
#include "../assets.h"   // For Asset
#include "../ui/button_config.h"  // For ButtonConfig
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>

// Forward declarations
//class Rectangle;
class Button;
class Bitmap;
class AnimatedBitmap;

struct DialogTextConfig {
    SDL_Rect rect;
    std::string text;
    SDL_Color color;
    bool centered = false;
};

struct DialogBitmap {
    Bitmap* bitmap;
    SDL_Rect rect;
    int zOrder = 0;
};

struct DialogConfig {
    SFont_Font *font;
    SDL_Rect bounds;
    SDL_Color backgroundColor = {0x60, 0x60, 0x60, 0xFF};
    SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<ButtonConfig> buttons;
    std::vector<DialogTextConfig> texts;
    std::vector<SDL_Rect> contentAreas;
    std::vector<SDL_Rect> lines;
    std::vector<Zone> zones;
    
    std::map<std::string, DialogBitmap> bitmaps;
    
    std::function<void(int)> onButtonPressed;
    std::function<void(Zone&)> onZoneHover;
};

class Dialog {
protected:
    Bitmap *mainBitmap;
    std::map<std::string, DialogBitmap> bitmaps;
    std::vector<Button*> buttons;
    std::vector<Rectangle*> contentAreas;
    DialogConfig config;
    bool isVisible = false;

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