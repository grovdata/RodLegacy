#include "bubble.h"
#include "../state.h"
#include "../parsers/libp.h"

Bubble::Bubble(string text, BubbleStyle style, int xPos, int yPos, bool autoDismiss) {
    string filename = "";

    AmigaLibAsset bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT;
    switch (style) {
        case BUBBLE_LEFT:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT;
            break;
        case BUBBLE_RIGHT:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_RIGHT;
            break;
        case BUBBLE_RIGHT_TWO_LINES:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT_TWO_ROWS;
            break;
    }

    Asset bubbleAsset = {LibAssetRegular, AssetLocationAmigaLibP, bubbleAssetId};
    bubble = state.assets.GetBitmapAsset(bubbleAsset);
    bubble->SetVisible(true);
    bubble->SetPosition(xPos, yPos);

    fontSurface.reset(SDL_CreateRGBSurface(0, 256, 64, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));

    UpdateText(text);

    if (autoDismiss) {
        timerStart = SDL_GetTicks64();
    }
}

Bubble::~Bubble() {
    // Free font surface
    delete bubble;
}

void Bubble::SetVisible(bool visible) {
    // Restart animation timer
    if (!this->visible && visible) {
        timerStart = SDL_GetTicks64();
    }
    this->visible = visible;
    //bubble->SetVisible(visible);
}

void Bubble::Draw() {

    uint64_t ticks = SDL_GetTicks64();

    if (ticks - timerStart > 5000)
        SetVisible(false);

    if (visible) {
        bubble->Draw();
        SDL_RenderCopy(renderer, fontTexture.get(), NULL, &fontRect);
    }

}

void Bubble::UpdateText(std::string text) {
    SDL_FillRect(fontSurface.get(), NULL, SDL_MapRGBA(fontSurface->format, 0, 0, 0, 0));
    
    char buf[50];
    snprintf(buf, 50, "%s", text.c_str());
    SFont_Write(fontSurface.get(), state.assets.GetMainFont(), 0, 0, buf);
    
    fontTexture.reset(SDL_CreateTextureFromSurface(renderer, fontSurface.get()));
    SDL_SetTextureColorMod(fontTexture.get(), 0, 0, 0);

    SDL_Rect rect = bubble->GetPosition();
    fontRect.x = rect.x; // dynamic
    fontRect.y = rect.y + 6;
    fontRect.w = fontSurface->w;
    fontRect.h = fontSurface->h;
}