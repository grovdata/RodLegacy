#include "bitmap.h"

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
    SDL_Log("Deleting Bitmap virtual");
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (surface) {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }
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

void Bitmap::LoadBitmapFromChunky(const std::vector<Color>& colors, int width, int height) {
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

    // Lock the surface to directly access the pixels
    SDL_LockSurface(surface);

    // Get a pointer to the surface's pixel data
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);

    // Iterate through the color vector and set each pixel on the surface
    for (int i = 0; i < width * height; ++i) {
        uint8_t r = std::get<0>(colors[i]);
        uint8_t g = std::get<1>(colors[i]);
        uint8_t b = std::get<2>(colors[i]);

        // Set the first pixel transparent
        uint8_t a = (i == 0) ? 0 : 255;

        // Pack the RGBA values into a 32-bit integer in SDL's format (RGBA)
        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
    }

    // Unlock the surface after pixel manipulation
    SDL_UnlockSurface(surface);

    // Set the color key (transparent pixel) if needed
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    // Create a texture from the surface
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "Error: Could not create SDL texture: " << SDL_GetError() << std::endl;
    }
}

void Bitmap::SaveBitmap(int index) {
    char filename[30];
    snprintf(filename, sizeof(filename),"sprite_%d.bmp", index);

    if (SDL_SaveBMP(surface, filename) != 0) {
        std::cerr << "Failed to save BMP file: " << SDL_GetError() << std::endl;
    }
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
