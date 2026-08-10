#pragma once
#include "../global.h"
#include "../SFont.h"
#include "../ui/zone.h"
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
    /** Optional: source sub-region of the bitmap's texture to draw; if w/h > 0, only this region is blitted to rect, leaving the underlying Bitmap untouched. */
    SDL_Rect sourceRect = {0, 0, 0, 0};
};

/**
 * Clips `logicalRect` (a bitmap's full, unscaled on-screen rect) to `mask`,
 * writing both the destination rect and a matching texture-space sourceRect
 * crop into db - e.g. keeping a part bitmap that slides/animates confined to
 * a window's porthole instead of spilling over its border art.
 *
 * Only correct for entries drawn at 1:1 scale (dest size == the bitmap's
 * natural size, i.e. no separate up/downscale baked into `rect`). Not valid
 * for AnimatedBitmap entries that pick a frame via their own grid selection
 * (SelectFrame) rather than a plain full-texture blit - DialogBitmap's
 * sourceRect-crop draw path bypasses that frame selection entirely.
 */
void ClipDialogBitmapToRect(DialogBitmap& db, const SDL_Rect& mask, SDL_Rect logicalRect);

struct DialogConfig {
    SFont_Font *font;
    SDL_Rect bounds;
    SDL_Color backgroundColor = {0x60, 0x60, 0x60, 0xFF};
    SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<ButtonConfig> buttons;
    std::vector<Button*> buttonPtrs;  // For directly passing Button/Checkbox instances
    std::vector<DialogTextConfig> texts;
    std::vector<SDL_Rect> contentAreas;
    std::vector<SDL_Rect> lines;
    std::vector<Zone> zones;
    
    std::map<std::string, DialogBitmap> bitmaps;

    std::function<void(int)> onButtonPressed;
    std::function<void(Zone&)> onZoneHover;
    std::function<void(Zone&)> onZoneClick;
    /** Optional: called when dialog is dismissed by Escape or click outside bounds. */
    std::function<void()> onDismiss;
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

    DialogBitmap* GetBitmap(const std::string& id);
}; 