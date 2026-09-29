/*
 * RodLegacy - Loads SDL surfaces from resources embedded in the binary.
 *
 * Created: 2025-10-03
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include "core/log.h"
#include <SDL.h>
#include <SDL_image.h>

// Caller frees the returned surface.
inline SDL_Surface* LoadSurfaceFromMemory(const unsigned char* data, size_t size) {
    SDL_RWops* rw = SDL_RWFromConstMem(data, size);
    if (!rw) {
        LOG_INFO("Failed to create RWops from memory: %s", SDL_GetError());
        return nullptr;
    }
    
    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) {
        LOG_INFO("Failed to load image from memory: %s", IMG_GetError());
        return nullptr;
    }
    
    return surface;
}

