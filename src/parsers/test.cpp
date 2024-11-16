#include "libp.h"
#include <SDL.h>
#include <vector>
#include <iostream>

// SDL_Surface* createSurfaceFromImageData(const std::vector<Color>& image_data, int width, int height) {
//     SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 16, 0xF800, 0x07E0, 0x001F, 0x0000);
//     if (!surface) {
//         std::cerr << "SDL_CreateRGBSurface failed: " << SDL_GetError() << std::endl;
//         return nullptr;
//     }

//     uint16_t* pixels = static_cast<uint16_t*>(surface->pixels);
//     for (int y = 0; y < height; ++y) {
//         for (int x = 0; x < width; ++x) {
//             int pixel_index = y * width + x;
//             pixels[pixel_index] = image_data[pixel_index];
//         }
//     }

//     return surface;
// }

SDL_Surface* createSurfaceFromColors(const std::vector<Color>& colors, int width, int height) {
    if (colors.size() != width * height) {
        std::cerr << "Error: The size of the color vector does not match the specified width and height." << std::endl;
        return nullptr;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Create an SDL surface with the specified width and height
    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (!surface) {
        std::cerr << "Error: Could not create SDL surface: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return nullptr;
    }

    // Lock the surface to directly access the pixels
    SDL_LockSurface(surface);

    // Get a pointer to the surface's pixel data
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);

    // Iterate through the color vector and set each pixel on the surface
    for (int i = 0; i < width * height; ++i) {
        uint8_t r = get<0>(colors[i]);
        uint8_t g = get<1>(colors[i]);
        uint8_t b = get<2>(colors[i]);

        // Pack the RGB values into a 32-bit integer in SDL's format (RGBA)
        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, 255);
    }

    // Unlock the surface after pixel manipulation
    SDL_UnlockSurface(surface);

    // Return the created SDL surface
    return surface;
}

int main(int argc, char *argv[]) {
    int spriteIndex = atoi(argv[1]);

    string examplefile = "../../../streetrod-bins/amiga/SR_disk2/lib.p";
    BitmapParser p;
    pair<int, vector<BitmapEntry_t>> entries = p.ReadEntries(examplefile.c_str());
    BitmapEntry_t b = entries.second[spriteIndex];
    vector<uint8_t> data = p.ReadImage(examplefile.c_str(), b, spriteIndex, entries.first);
    Palette pal = p.PreparePalette(PAL1);

    vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);
    // p.PrintBitplanes(data);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Surface* surface = createSurfaceFromColors(chunkyData, b.width, b.height);

    if (surface) {
        // Use the surface as needed, for example save it to a BMP file
        SDL_SaveBMP(surface, "output.bmp");
        SDL_FreeSurface(surface);
    }

    SDL_Quit();
}