#include "bubble.h"

using namespace std;

Bubble::Bubble(string text, BubbleStyle style, int xPos, int yPos) {
    string filename = "";
    switch (style) {
        case BUBBLE_LEFT:
            filename = "lib_p_0.gif";
            break;
        case BUBBLE_RIGHT:
            filename = "lib_p_1.gif";
            break;
        case BUBBLE_RIGHT_TWO_LINES:
            filename = "lib_p_2.gif";
            break;
    }

    // FIXME
    filename = "../data/res/bubbles/" + filename;

    LoadBitmap(filename);
    rect.x = xPos;
    rect.y = yPos;

    fontSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    UpdateText(text, xPos, yPos);
}

Bubble::~Bubble() {
    // Free font surface
    Bitmap::~Bitmap();
}

void Bubble::Draw() {
    Bitmap::Draw();
    if (isVisible)
        SDL_RenderCopy(renderer, fontTexture, NULL, &fontRect);

}

void Bubble::UpdateText(std::string text, int xPos, int yPos) {
    char buf[50];
    snprintf(buf, 50, "%s", text.c_str());
    SFont_Write(fontSurface, font, 0, 0, buf);
    fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
    SDL_SetTextureColorMod(fontTexture, 0, 0, 0);

    fontRect.x = rect.x + 6; // dynamic
    fontRect.y = rect.y + 6;
    fontRect.w = fontSurface->w;
    fontRect.h = fontSurface->h;
}