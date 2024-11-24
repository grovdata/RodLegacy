#include "debug_overlay.h"

DebugOverlay::DebugOverlay(SDL_Renderer* r, SFont_Font* f) 
    : font(f), padding(5), needsUpdate(true) {
    renderer = r;
    bgColor = {0, 0, 0, 51}; // 20% opacity black
}

DebugOverlay::~DebugOverlay() {
    // Parent Bitmap destructor will handle surface and texture cleanup
}

void DebugOverlay::Clear() {
    lines.clear();
    needsUpdate = true;
}

void DebugOverlay::AddLine(const std::string& text) {
    lines.push_back(text);
    needsUpdate = true;
}

void DebugOverlay::UpdateContent() {
    if (lines.empty()) return;

    // Calculate required dimensions
    int totalHeight = 0;
    int maxWidth = 0;
    for (const auto& line : lines) {
        int w = SFont_TextWidth(font, line.c_str());
        maxWidth = std::max(maxWidth, w);
        totalHeight += SFont_TextHeight(font);
    }

    int width = maxWidth + (padding * 2);
    int height = totalHeight + (padding * 2);

    // Create/recreate surface if needed
    if (!surface || surface->w != width || surface->h != height) {
        if (surface) {
            SDL_FreeSurface(surface);
        }
        surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    }

    // Fill background
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, bgColor.r, bgColor.g, bgColor.b, bgColor.a));

    // Draw text
    int y = padding;
    for (const auto& line : lines) {
        SFont_Write(surface, font, padding, y, line.c_str());
        y += SFont_TextHeight(font);
    }

    // Update texture
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    // Update rect position
    rect.w = width;
    rect.h = height;
    rect.x = GAME_WIDTH - width;
    rect.y = padding;

    needsUpdate = false;
}

void DebugOverlay::Draw() {
    if (lines.empty()) return;

    if (needsUpdate) {
        UpdateContent();
    }

    if (isVisible && texture) {
        SDL_RenderCopy(renderer, texture, NULL, &rect);
    }
} 