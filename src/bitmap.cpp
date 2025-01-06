#include "bitmap.h"

extern "C" {
#include "lodepng.h"
}

using namespace std;

Bitmap::Bitmap() {
    surface = NULL;
    texture = NULL;
    isVisible = true;
    rect = {0, 0, 0, 0};
}

Bitmap::Bitmap(int width, int height, int depth) {
    rect = {0, 0, width, height};
    surface = SDL_CreateRGBSurface(0, width, height, depth, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}

Bitmap::~Bitmap() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (surface) {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }
}

 // Assignment from pointer
Bitmap& Bitmap::operator=(const Bitmap* other) {
    if (other) {
        // Clean up existing resources
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }

        // Copy from other
        if (other->surface) {
            surface = SDL_CreateRGBSurfaceFrom(other->surface->pixels,
                                                other->surface->w,
                                                other->surface->h,
                                                other->surface->format->BitsPerPixel,
                                                other->surface->pitch,
                                                other->surface->format->Rmask,
                                                other->surface->format->Gmask,
                                                other->surface->format->Bmask,
                                                other->surface->format->Amask);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
        }
        isVisible = other->isVisible;
        rect = other->rect;
    }
    return *this;
}

// Regular assignment operator
Bitmap& Bitmap::operator=(const Bitmap& other) {
    if (this != &other) {
        // Clean up existing resources
        if (texture) {
            SDL_DestroyTexture(texture);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }

        // Copy from other
        if (other.surface) {
            surface = SDL_CreateRGBSurfaceFrom(other.surface->pixels,
                                                other.surface->w,
                                                other.surface->h,
                                                other.surface->format->BitsPerPixel,
                                                other.surface->pitch,
                                                other.surface->format->Rmask,
                                                other.surface->format->Gmask,
                                                other.surface->format->Bmask,
                                                other.surface->format->Amask);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
        }
        isVisible = other.isVisible;
        rect = other.rect;
    }
    return *this;
}

SDL_Surface* Bitmap::GetSurface() {
    return surface;
}

SDL_Texture* Bitmap::GetTexture() {
    return texture;
}

SDL_Rect Bitmap::GetPosition() {
    return this->rect;
}

void Bitmap::SetPosition(int x, int y) {
    this->rect.x = x;
    this->rect.y = y;
}

void Bitmap::SetDimensions(int width, int height) {
    this->rect.w = width;
    this->rect.h = height;
}

void Bitmap::Align(BitmapAlign b, bool allowStatusBar) {
    int offset = allowStatusBar ? GAME_STATUSBAR_HEIGHT : 0;
    if (b == BitmapAlignTop) {
        this->rect.y = 0 + offset;
    } else {
        this->rect.y = GAME_HEIGHT - this->rect.h - offset;
    }
}

void Bitmap::AlignAbove(Bitmap *dest) {
    this->rect.y = dest->GetPosition().y - this->rect.h;
}

void Bitmap::Crop(SDL_Rect crop) {
    // Create a new surface with the dimensions of the crop rectangle
    SDL_Surface* cropped = SDL_CreateRGBSurface(
        0,
        crop.w,
        crop.h,
        surface->format->BitsPerPixel,
        surface->format->Rmask,
        surface->format->Gmask,
        surface->format->Bmask,
        surface->format->Amask
    );

    if (!cropped) {
        std::cerr << "SDL_CreateRGBSurface failed: " << SDL_GetError() << std::endl;
        return;
    }

    // Copy the pixels from the original surface to the new cropped surface
    SDL_BlitSurface(surface, &crop, cropped, nullptr);
    SDL_FreeSurface(surface);

    // Free existing texture before creating new one
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }

    surface = cropped;
    texture = SDL_CreateTextureFromSurface(renderer, cropped);
}

void Bitmap::CreateMask(SDL_Surface *mask, SDL_Surface *target, SDL_Rect rect) {
    Color maskColor = AmigaMaskColor;
    uint8_t maskR = std::get<0>(maskColor);
    uint8_t maskG = std::get<1>(maskColor);
    uint8_t maskB = std::get<2>(maskColor);

    SDL_Log("Looking for mask color RGB(%d,%d,%d)", maskR, maskG, maskB);
    SDL_Log("First pixel comparison: Mask RGB(128,16,0) == Target RGB(%d,%d,%d): %s", 
            maskR, maskG, maskB,
            (128 == maskR && 16 == maskG && 0 == maskB) ? "true" : "false");

    if (target->format != surface->format) {
        SDL_LogWarn(0, "Target surface format differs from source format - this might cause issues");
        SDL_LogWarn(0, "Source BPP: %d, Target BPP: %d", surface->format->BitsPerPixel, target->format->BitsPerPixel);
    }

    // Ensure the rect does not exceed the bounds of the surfaces
    if (rect.x < 0 || rect.y < 0 || 
        rect.x + rect.w > surface->w || rect.y + rect.h > surface->h) {
        SDL_LogError(0, "Rectangle exceeds the bounds of the surface surface!\n");
        return;
    }

    if (rect.w > mask->w || rect.h > mask->h) {
        SDL_LogError(0, "Mask dimensions are smaller than the rectangle area!\n");
        return;
    }

    // Lock surfaces for pixel access if necessary
    if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);
    if (SDL_MUSTLOCK(mask)) SDL_LockSurface(mask);
    if (SDL_MUSTLOCK(target)) SDL_LockSurface(target);

    // Access pixel data
    Uint32 *bgPixels = (Uint32 *)surface->pixels;
    Uint32 *maskPixels = (Uint32 *)mask->pixels;
    Uint32 *targetPixels = (Uint32 *)target->pixels;

    SDL_PixelFormat *maskFormat = mask->format;

    int copiedPixels = 0;
    static int debugCount = 0;
    debugCount = 0;  // Reset debug counter

    for (int y = 0; y < rect.h; y++) {
        for (int x = 0; x < rect.w; x++) {
            int maskIdx = y * mask->w + x;
            int bgIdx = (rect.y + y) * surface->w + (rect.x + x);
            int targetIdx = y * target->w + x;  // Changed to use local coordinates

            Uint32 maskPixel = maskPixels[maskIdx];
            Uint8 r, g, b;
            SDL_GetRGB(maskPixel, maskFormat, &r, &g, &b);

            if (r == maskR && g == maskG && b == maskB) {
                Uint8 bgR, bgG, bgB;
                SDL_GetRGB(bgPixels[bgIdx], surface->format, &bgR, &bgG, &bgB);
                targetPixels[targetIdx] = SDL_MapRGBA(target->format, bgR, bgG, bgB, 255);
                copiedPixels++;
            } else {
                targetPixels[targetIdx] = SDL_MapRGBA(target->format, 0, 0, 0, 0);
            }
        }
    }

    // Unlock surfaces
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    if (SDL_MUSTLOCK(mask)) SDL_UnlockSurface(mask);
    if (SDL_MUSTLOCK(target)) SDL_UnlockSurface(target);
}


void Bitmap::SetVisible(bool visible) {
    this->isVisible = visible;
}

void Bitmap::LoadBitmap(string filename) {
    surface = IMG_Load(strdup(filename.c_str()));
    //SDL_SetColorKey(surface, 0, SDL_MapRGB(surface->format, 0, 0, 0));
    if (surface == NULL) {
        SDL_Log("Bitmap::LoadBitmap error, could not load surface %s\n", filename.c_str());
        return;
    } else {
        SDL_LogDebug(0, "Loaded bitmap %s, size %d x %d\n", filename.c_str(), surface->w, surface->h);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;
}

void Bitmap::LoadSurface(SDL_Surface* src) {
    if (src == nullptr) {
        std::cerr << "Source surface is null!" << std::endl;
        return;
    }

    // Create a new surface with the same format and size as the source
    surface = SDL_CreateRGBSurfaceWithFormat(
        0, src->w, src->h, src->format->BitsPerPixel, src->format->format);

    if (surface == nullptr) {
        std::cerr << "Could not create surface: " << SDL_GetError() << std::endl;
        return;
    }

    // Copy the pixel data
    if (SDL_BlitSurface(src, nullptr, surface, nullptr) < 0) {
        std::cerr << "Could not copy surface: " << SDL_GetError() << std::endl;
        return;
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "Error: Could not create SDL texture: " << SDL_GetError() << std::endl;
    }
}

void Bitmap::LoadBitmapFromChunky(const std::vector<Color>& colors, int width, int height, bool isMask) {
    if (colors.size() != width * height) {
        std::cerr << "Error: The size of the color vector does not match the specified width and height." << std::endl;
        return;
    }

    // Create an SDL surface with the specified width and height
    surface = SDL_CreateRGBSurface(0, width, height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (!surface) {
        std::cerr << "Error: Could not create SDL surface: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_LockSurface(surface);
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);

    for (int i = 0; i < width * height; ++i) {
        uint8_t r = std::get<0>(colors[i]);
        uint8_t g = std::get<1>(colors[i]);
        uint8_t b = std::get<2>(colors[i]);
        
        uint8_t a;
        if (isMask) {
            // For masks: any non-zero pixel becomes transparent (alpha = 0)
            // Zero pixels become opaque black (alpha = 255)
            a = (r || g || b) ? 0 : 255;
            r = g = b = 0;  // Set color to black for mask pixels
        } else {
            // Normal bitmap behavior: first pixel transparent, rest opaque
            a = (r == 0 && g == 0 && b == 0) ? 0 : 255;
        }

        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
    }

    SDL_UnlockSurface(surface);

    // Set up alpha blending for the texture
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "Error: Could not create SDL texture: " << SDL_GetError() << std::endl;
    } else {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
}

void Bitmap::SaveBitmap(int index) {
    char filename[30];
    snprintf(filename, sizeof(filename), "sprite_%d.png", index);

    // Get surface pixels and convert to RGBA format
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgbaSurface) {
        std::cerr << "Failed to convert surface: " << SDL_GetError() << std::endl;
        return;
    }

    // Get pixel data
    const size_t pixelCount = rgbaSurface->w * rgbaSurface->h * 4;
    uint8_t* image = new uint8_t[pixelCount];
    
    SDL_LockSurface(rgbaSurface);
    memcpy(image, rgbaSurface->pixels, pixelCount);
    SDL_UnlockSurface(rgbaSurface);

    // Encode and save using lodepng
    unsigned error = lodepng_encode32_file(filename, image, rgbaSurface->w, rgbaSurface->h);
    if (error) {
        std::cerr << "Failed to save PNG file: " << lodepng_error_text(error) << std::endl;
    }

    delete[] image;
    SDL_FreeSurface(rgbaSurface);
}


void Bitmap::Draw() {
    if (isVisible && texture != NULL) {
        SDL_RenderCopy(renderer, texture, NULL, &rect);
    }
}

void Bitmap::UpdateFromSurface() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}
