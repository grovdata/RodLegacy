// Helper functions for loading SDL resources from embedded binary data
#pragma once
#include <SDL.h>
#include <SDL_image.h>

/**
 * Load an SDL_Surface from embedded binary data (e.g., PNG bytes)
 * 
 * @param data Pointer to embedded binary data
 * @param size Size of the data in bytes
 * @return SDL_Surface* or nullptr on failure
 * 
 * Note: The returned surface must be freed by the caller
 */
inline SDL_Surface* LoadSurfaceFromMemory(const unsigned char* data, size_t size) {
    // Create RWops from constant memory
    SDL_RWops* rw = SDL_RWFromConstMem(data, size);
    if (!rw) {
        SDL_Log("Failed to create RWops from memory: %s", SDL_GetError());
        return nullptr;
    }
    
    // Load image from RWops (1 = auto-close RWops after load)
    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) {
        SDL_Log("Failed to load image from memory: %s", IMG_GetError());
        return nullptr;
    }
    
    return surface;
}

