#pragma once

#include <vector>
#include <string>
#include "global.h"
#include "bitmap.h"
#include "SFont.h"

class DebugOverlay : public Bitmap {
private:
    SFont_Font* font;
    std::vector<std::string> lines;
    int padding;
    SDL_Color bgColor;
    bool needsUpdate;

    void UpdateContent();

public:
    DebugOverlay(SDL_Renderer* r, SFont_Font* f);
    ~DebugOverlay();
    
    void Clear();
    void AddLine(const std::string& text);
    void Draw() override;
    
    void SetPadding(int pad) { padding = pad; }
    void SetBackgroundColor(SDL_Color color) { bgColor = color; }
    void SetBackgroundOpacity(Uint8 opacity) { bgColor.a = opacity; }
};
