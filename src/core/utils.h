#pragma once
#include <set>
#include <stdexcept>  // std::out_of_range
#include <random>
#include <memory>

// SFont clips the last glyph when a surface is sized to the exact text width;
// padding the surface by this many pixels avoids it.
constexpr int kSFontClipPad = 2;

inline std::mt19937& RandomEngine() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

template <typename T>
T getRandomElement(const std::set<T>& inputSet) {
    if (inputSet.empty()) {
        // Handle the case when the set is empty
        throw std::out_of_range("The set is empty");
    }

    // Use random_device to seed the random number generator
    std::random_device rd;
    // Use the Mersenne Twister engine for randomness
    std::mt19937 gen(rd());

    // Use uniform_int_distribution to get a random index within the set size
    std::uniform_int_distribution<size_t> dist(0, inputSet.size() - 1);

    // Advance to the random element using an iterator
    auto it = inputSet.begin();
    std::advance(it, dist(gen));

    // Return the random element
    return *it;
}

// https://swarminglogic.com/scribble/2015_05_smartwrappers
namespace sdl2 {
    struct SDL_Deleter {
        void operator()(SDL_Surface*  ptr) { if (ptr) SDL_FreeSurface(ptr); }
        void operator()(SDL_Texture*  ptr) { if (ptr) SDL_DestroyTexture(ptr); }
        void operator()(SDL_Renderer* ptr) { if (ptr) SDL_DestroyRenderer(ptr); }
        void operator()(SDL_Window*   ptr) { if (ptr) SDL_DestroyWindow(ptr); }
        void operator()(SDL_RWops*    ptr) { if (ptr) SDL_RWclose(ptr); }
    };

    // Unique Pointers
    using SurfacePtr  = std::unique_ptr<SDL_Surface,  SDL_Deleter>;
    using TexturePtr  = std::unique_ptr<SDL_Texture,  SDL_Deleter>;
    using RendererPtr = std::unique_ptr<SDL_Renderer, SDL_Deleter>;
    using WindowPtr   = std::unique_ptr<SDL_Window,   SDL_Deleter>;
    using RWopsPtr    = std::unique_ptr<SDL_RWops,    SDL_Deleter>;
}