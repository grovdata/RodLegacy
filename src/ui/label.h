#pragma once

#include "bitmap.h"
#include "../SFont.h"
#include <SDL.h>
#include <vector>

class Label : public Bitmap {
private:
    SFont_Font* font;
    uint32_t textColor;
    int lineHeight;
    vector<string> textLines;
    
    void splitIntoLines(const string& text);

public:
    Label();
    ~Label();
    
    void Init(SFont_Font* font, int xPos, int yPos, uint32_t textColor);
    void SetText(string text, bool centered = false, int desiredWidth = 0);
};