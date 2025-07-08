#include "label.h"
#include <sstream>

Label::Label() : Bitmap() {
}

Label::~Label() {
    Bitmap::~Bitmap();
}

void Label::Init(SFont_Font *font, int xPos, int yPos, uint32_t textColor) {
    this->font = font;
    this->textColor = textColor;
    this->lineHeight = SFont_TextHeight(font);
    SetPosition(xPos, yPos);
    SetVisible(true);
}

void Label::splitIntoLines(const std::string& text) {
    textLines.clear();
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        textLines.push_back(line);
    }
}

void Label::SetText(std::string text, bool centered, int desiredWidth) {
    splitIntoLines(text);
    
    // Calculate total height and maximum width
    int totalHeight = lineHeight * textLines.size();
    int maxWidth = 0;
    
    for (const auto& line : textLines) {
        int lineWidth = SFont_TextWidth(font, line.c_str());
        maxWidth = std::max(maxWidth, lineWidth);
    }
    
    // Use the larger of maxWidth or desiredWidth
    int surfaceWidth = (desiredWidth > 0) ? desiredWidth : maxWidth;
    
    surface = SDL_CreateRGBSurface(0, surfaceWidth, totalHeight, 32, 
                                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SetDimensions(surfaceWidth, totalHeight);
    
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    
    // Render each line
    int yOffset = 0;
    for (const auto& line : textLines) {
        int x = 0; // Base offset for SFont bug workaround
        
        if (centered && desiredWidth > 0) {
            int lineWidth = SFont_TextWidth(font, line.c_str());
            x += (desiredWidth - lineWidth) / 2;
        }
        
        char buf[50];
        snprintf(buf, 50, "%s", line.c_str());
        SFont_Write(surface, font, x, yOffset, buf);
        yOffset += lineHeight;
    }
    
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    uint8_t r = (textColor >> 16) & 0xFF;
    uint8_t g = (textColor >> 8) & 0xFF;
    uint8_t b = textColor & 0xFF;
    SDL_SetTextureColorMod(texture, r, g, b);
}