#include "label.h"

Label::Label() : Bitmap() {
}

Label::~Label() {
    Bitmap::~Bitmap();
}

void Label::Init(SFont_Font *font, int xPos, int yPos, uint32_t textColor) {
    this->font = font;
    this->textColor = textColor;
    SetPosition(xPos, yPos);
    SetVisible(true);
}

void Label::SetText(string text, bool centered, int desiredWidth = 0) {
    int textWidth = SFont_TextWidth(font, text.c_str());
    int textHeight = SFont_TextHeight(font);
    char buf[50];
    int x = -10; // Base offset for SFont bug workaround
    
    // Use the larger of textWidth or desiredWidth
    int surfaceWidth = (desiredWidth > 0) ? desiredWidth : textWidth;
    
    // Calculate centering offset if needed
    if (centered && desiredWidth > 0) {
        x += (desiredWidth - textWidth) / 2;
    }

    surface = SDL_CreateRGBSurface(0, surfaceWidth, textHeight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SetDimensions(surfaceWidth, textHeight);

    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    
    snprintf(buf, 50, "%s", text.c_str());
    SFont_Write(surface, font, x, 0, buf);

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    uint8_t r = (textColor >> 16) & 0xFF;
    uint8_t g = (textColor >> 8) & 0xFF;
    uint8_t b = textColor & 0xFF;
    SDL_SetTextureColorMod(texture, r, g, b);
}


void Label::Draw() {
    Bitmap::Draw();
    SDL_RenderCopy(renderer, texture, NULL, &rect);
}