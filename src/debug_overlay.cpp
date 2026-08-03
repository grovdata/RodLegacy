#include "debug_overlay.h"

DebugOverlay::DebugOverlay(SDL_Renderer* r, SFont_Font* f) 
    : font(f), padding(5), needsUpdate(true) {
    renderer = r;
    bgColor = {0, 0, 0, 150}; // 20% opacity black
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
        int w = SFontUtils::GetTextWidth(font, line);
        maxWidth = std::max(maxWidth, w);
        totalHeight += SFont_TextHeight(font);
    }

    // Smooth width changes to prevent jumping
    maxWidth = std::max(maxWidth, minWidth);
    if (maxWidth < lastWidth) {
        // Only shrink if new content is significantly smaller (prevents rapid oscillation)
        if (maxWidth < lastWidth * 0.7) {
            lastWidth = maxWidth;
        } else {
            maxWidth = lastWidth;
        }
    } else {
        lastWidth = maxWidth;
    }

    int width = maxWidth + (padding * 2);
    int height = totalHeight + (padding * 2);

    // Create/recreate surface if needed
    if (!surface || surface->w != width || surface->h != height) {
        surface.reset();  // RAII handles cleanup
        surface = sdl2::SurfacePtr(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32));
    }

    // Fill background
    SDL_FillRect(surface.get(), NULL, SDL_MapRGBA(surface->format, bgColor.r, bgColor.g, bgColor.b, bgColor.a));

    // Draw text
    int y = padding;
    for (const auto& line : lines) {
        SFontUtils::Write(surface.get(), font, padding, y, line);
        y += SFont_TextHeight(font);
    }

    // Update texture
    texture.reset();  // RAII handles cleanup
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);

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
        SDL_RenderCopy(renderer, texture.get(), NULL, &rect);
    }
} 