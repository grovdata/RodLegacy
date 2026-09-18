#include "ui/bubble.h"
#include "core/state.h"
#include "parsers/libp.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace {
// Text layout inside a box, from SetupMessageBox (0x00222742-0x002227aa):
// first row at box.y + 6, every row centred across the box's full width,
// 9 pixels apart.
constexpr int kTextTopInset = 6;
constexpr int kRowHeight = 9;
}

Bubble::Bubble(std::string text, BubbleStyle style, bool autoDismiss) {
    ApplyStyle(style);
    UpdateText(text);

    if (autoDismiss) {
        timerStart = SDL_GetTicks64();
    }
}

Bubble::~Bubble() {
    // bubble is owned by AssetManager; fontSurface/fontTexture are managed by smart pointers
}

void Bubble::ApplyStyle(BubbleStyle style) {
    this->style = style;

    AmigaLibAsset bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT;
    int xPos = 80, yPos = 30;
    switch (style) {
        case BUBBLE_LEFT:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT;
            xPos = 80; yPos = 30;
            break;
        case BUBBLE_RIGHT:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_RIGHT;
            xPos = 72; yPos = 30;
            break;
        case BUBBLE_RIGHT_TWO_LINES:
            bubbleAssetId = SPEECH_BUBBLE_ARROW_LEFT_TWO_ROWS;
            xPos = 80; yPos = 20;
            break;
    }

    Asset bubbleAsset = {LibAssetRegular, SrLibFileAmigaLibP, bubbleAssetId};
    bubble = state.GetAssets().GetBitmapAsset(bubbleAsset);
    bubble->SetVisible(true);
    bubble->SetPosition(xPos, yPos);
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

void Bubble::UpdateText(std::string& text) {
    SFont_Font* font = state.GetAssets().GetMainFont();

    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back("");

    SDL_Rect box = bubble->GetNaturalSize();

    // A line wider than the box would be clipped by the surface, so grow the
    // surface and keep it centred on the box: overflow then spills evenly to
    // both sides instead of vanishing.
    int widest = 0;
    for (const auto& l : lines) {
        widest = std::max(widest, SFont_TextWidth(font, l.c_str()) + kSFontClipPad);
    }
    int surfaceW = std::max(box.w, widest);
    int pad = (surfaceW - box.w) / 2;
    int surfaceH = kTextTopInset + kRowHeight * (int)lines.size();

    fontSurface.reset(SDL_CreateRGBSurface(0, surfaceW, surfaceH, 32,
                                           0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
    SDL_FillRect(fontSurface.get(), NULL, SDL_MapRGBA(fontSurface->format, 0, 0, 0, 0));

    int y = kTextTopInset;
    for (const auto& l : lines) {
        int w = SFont_TextWidth(font, l.c_str());
        SFont_Write(fontSurface.get(), font, pad + (box.w - w) / 2, y, l.c_str());
        y += kRowHeight;
    }

    fontTexture.reset(SDL_CreateTextureFromSurface(renderer, fontSurface.get()));
    SDL_SetTextureColorMod(fontTexture.get(), 0, 0, 0);

    SDL_Rect rect = bubble->GetPosition();
    fontRect.x = rect.x - pad;
    fontRect.y = rect.y;
    fontRect.w = fontSurface->w;
    fontRect.h = fontSurface->h;
}
