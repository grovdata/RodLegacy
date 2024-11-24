#include "colorizedbitmap.h"
void ColorizedBitmap::ApplyPalette(StandardCarColor palette) {
    if (currentPalette == palette) {
        return;  // Already using this palette
    }
    
    // Free existing surface and texture
    if (surface) {
        SDL_FreeSurface(surface);
    }
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    
    // Create new surface by copying from base bitmap
    surface = SDL_CreateRGBSurfaceWithFormat(0, 
        baseBitmap->GetPosition().w,
        baseBitmap->GetPosition().h,
        32, SDL_PIXELFORMAT_RGBA32);
    
    // Copy base surface
    SDL_BlitSurface(baseBitmap->GetSurface(), nullptr, surface, nullptr);
    
    // Apply palette transformation
    vector<uint16_t> carPalette = GetStandardCarColor(palette);
    // ... apply palette to surface ...
    
    // Create new texture
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    currentPalette = palette;
    
    // Copy position from base bitmap
    rect = baseBitmap->GetPosition();
    isVisible = baseBitmap->IsVisible();
} 