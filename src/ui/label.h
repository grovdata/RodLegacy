#pragma once

#include "bitmap.h"
#include "../SFont.h"
#include <SDL.h>

class Label : public Bitmap {
private:
    SFont_Font *font;
    uint32_t textColor;
public:
    Label();
    void Init(SFont_Font *font, int xPos, int yPos, uint32_t textColor);
    ~Label();

    void SetText(string text, bool centered);
    void Draw();
};