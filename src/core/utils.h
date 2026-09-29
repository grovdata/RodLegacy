/*
 * RodLegacy - Small helpers: the seeded random engine, SDL smart-pointer wrappers and the SFont clip padding.
 *
 * Created: 2024-01-20
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <set>
#include <stdexcept>
#include <random>
#include <memory>

// SFont clips the last glyph when a surface is sized to the exact text width.
constexpr int kSFontClipPad = 2;

inline std::mt19937& RandomEngine() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

template <typename T>
T getRandomElement(const std::set<T>& inputSet) {
    if (inputSet.empty()) {
        throw std::out_of_range("The set is empty");
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<size_t> dist(0, inputSet.size() - 1);

    auto it = inputSet.begin();
    std::advance(it, dist(gen));

    return *it;
}

namespace sdl2 {
    struct SDL_Deleter {
        void operator()(SDL_Surface*  ptr) { if (ptr) SDL_FreeSurface(ptr); }
        void operator()(SDL_Texture*  ptr) { if (ptr) SDL_DestroyTexture(ptr); }
        void operator()(SDL_Renderer* ptr) { if (ptr) SDL_DestroyRenderer(ptr); }
        void operator()(SDL_Window*   ptr) { if (ptr) SDL_DestroyWindow(ptr); }
        void operator()(SDL_RWops*    ptr) { if (ptr) SDL_RWclose(ptr); }
    };

    using SurfacePtr  = std::unique_ptr<SDL_Surface,  SDL_Deleter>;
    using TexturePtr  = std::unique_ptr<SDL_Texture,  SDL_Deleter>;
    using RendererPtr = std::unique_ptr<SDL_Renderer, SDL_Deleter>;
    using WindowPtr   = std::unique_ptr<SDL_Window,   SDL_Deleter>;
    using RWopsPtr    = std::unique_ptr<SDL_RWops,    SDL_Deleter>;
}