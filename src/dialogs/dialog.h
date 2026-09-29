/*
 * RodLegacy - Modal dialogs: Dialog with its texts, bitmaps, buttons, zones and keyboard handling.
 *
 * Created: 2024-12-28
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include "core/global.h"
#include "gfx/SFont.h"
#include "ui/zone.h"
#include "ui/rectangle.h"
#include "core/assets.h"
#include "ui/button_config.h"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>

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

// Clips a 1:1 bitmap to `mask`, writing the destination rect and matching sourceRect into db.
// Not valid for AnimatedBitmap frames: the sourceRect path bypasses frame selection.
void ClipDialogBitmapToRect(DialogBitmap& db, const SDL_Rect& mask, SDL_Rect logicalRect);

struct DialogConfig {
    SFont_Font *font;
    SDL_Rect bounds;
    SDL_Color backgroundColor = {0x60, 0x60, 0x60, 0xFF};
    SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<ButtonConfig> buttons;
    std::vector<Button*> buttonPtrs;
    std::vector<DialogTextConfig> texts;
    std::vector<SDL_Rect> contentAreas;
    std::vector<SDL_Rect> lines;
    std::vector<Zone> zones;
    
    std::map<std::string, DialogBitmap> bitmaps;

    std::function<void(int)> onButtonPressed;
    std::function<void(Zone&)> onZoneHover;
    std::function<void(Zone&)> onZoneClick;
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
    // Set by subclasses that draw into mainBitmap themselves, so Draw() blits it even without config content.
    bool mainBitmapHasContent = false;

public:
    Dialog(const DialogConfig& cfg);
    virtual ~Dialog();
    
    void AddContentArea(const SDL_Rect& area, SDL_Color fillColor = {0x60, 0x60, 0x60, 0xFF}, SDL_Color borderColor = {0xFF, 0xFF, 0xFF, 0xFF});
    void AddButton(const ButtonConfig& buttonCfg);
    void AddText(const char* text, const SDL_Rect& area);
    void AddLine(SDL_Rect rect, SDL_Color color);

    virtual bool HandleEvent(SDL_Event *event);
    virtual void Draw();
    virtual void Show();
    virtual void Hide();
    virtual bool IsVisible() const { return isVisible; }
    void OnButtonPressed(int index);

    DialogBitmap* GetBitmap(const std::string& id);
    Zone* GetZone(const std::string& id);

    std::vector<SDL_Rect> NavTargets() const;
}; 