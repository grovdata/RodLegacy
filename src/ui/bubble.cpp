#include "ui/bubble.h"
#include "core/state.h"
#include "parsers/libp.h"
#include <algorithm>
#include <sstream>
#include <vector>

Bubble::Bubble(std::string text, BubbleStyle style, int xPos, int yPos, bool autoDismiss) {
    std::string filename = "";

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

    Asset bubbleAsset = {LibAssetRegular, SrLibFileAmigaLibP, bubbleAssetId};
    bubble = state.GetAssets().GetBitmapAsset(bubbleAsset);
    bubble->SetVisible(true);
    bubble->SetPosition(xPos, yPos);

    UpdateText(text);

    if (autoDismiss) {
        timerStart = SDL_GetTicks64();
    }
}

Bubble::~Bubble() {
    // bubble is owned by AssetManager; fontSurface/fontTexture are managed by smart pointers
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
    int lineHeight = SFont_TextHeight(font);

    // Split on \n (same convention as Label::splitIntoLines) so callers can
    // request a multi-line bubble (e.g. paired with BUBBLE_RIGHT_TWO_LINES).
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back("");

    // Size the surface to the widest line so nothing is clipped. Previously a
    // fixed 256x64 surface (and a 50-char buffer) cropped longer text.
    // kSFontClipPad avoids SFont clipping the last glyph (same workaround as Label).
    int textW = 0;
    for (const auto& l : lines) {
        textW = std::max(textW, SFont_TextWidth(font, l.c_str()));
    }
    textW += kSFontClipPad;
    int textH = lineHeight * (int)lines.size();
    fontSurface.reset(SDL_CreateRGBSurface(0, textW, textH, 32,
                                           0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));

    SDL_FillRect(fontSurface.get(), NULL, SDL_MapRGBA(fontSurface->format, 0, 0, 0, 0));
    int y = 0;
    for (const auto& l : lines) {
        SFont_Write(fontSurface.get(), font, 0, y, l.c_str());
        y += lineHeight;
    }

    fontTexture.reset(SDL_CreateTextureFromSurface(renderer, fontSurface.get()));
    SDL_SetTextureColorMod(fontTexture.get(), 0, 0, 0);

    // Inset the text from the bubble's left/top edge so it doesn't butt against
    // the border.
    constexpr int kTextPadX = 8;
    constexpr int kTextPadY = 6;
    SDL_Rect rect = bubble->GetPosition();
    fontRect.x = rect.x + kTextPadX;
    fontRect.y = rect.y + kTextPadY;
    fontRect.w = fontSurface->w;
    fontRect.h = fontSurface->h;
}