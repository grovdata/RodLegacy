#pragma once

#include <vector>
#include <string>
#include "global.h"
#include "bitmap.h"
#include "SFont.h"

// Add string wrapper functions
namespace SFontUtils {
    inline int GetTextWidth(const SFont_Font* font, const std::string& text) {
        return SFont_TextWidth(font, text.c_str());
    }

    inline void Write(SDL_Surface* surface, const SFont_Font* font, int x, int y, const std::string& text) {
        SFont_Write(surface, font, x, y, text.c_str());
    }

    inline void WriteCenter(SDL_Surface* surface, const SFont_Font* font, int y, const std::string& text) {
        SFont_WriteCenter(surface, font, y, text.c_str());
    }
}

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
