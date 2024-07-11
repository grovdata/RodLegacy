#include "bitmap.h"

using namespace std;

Bitmap::Bitmap() {
}

Bitmap::~Bitmap() {
    if (surface != NULL) {
        SDL_FreeSurface(surface);
    }
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
}

SDL_Rect Bitmap::GetPosition() {
    return this->rect;
}

void Bitmap::SetPosition(int x, int y) {
    this->rect.x = x;
    this->rect.y = y;
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

Bitmap::LoadBitmapFromChunky(const vector<uint16_t>& image_data, int width, int height) {
    surface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) {
        cerr << "LoadBitmapFromChunky failed: " << SDL_GetError() << endl;
        return nullptr;
    }

    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixel_index = y * width + x;
            uint16_t color = image_data[pixel_index];

            // Convert the 16-bit color to 24-bit RGB
            uint8_t r = (color & 0xF800) >> 8; // 5 bits
            uint8_t g = (color & 0x07E0) >> 3; // 6 bits
            uint8_t b = (color & 0x001F) << 3; // 5 bits

            // Combine into a 32-bit color value
            uint32_t rgb_color = (r << 16) | (g << 8) | b;

            // Set the pixel in the surface
            pixels[pixel_index] = rgb_color;
        }
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
}


void Bitmap::Draw() {
    if (isVisible)
        SDL_RenderCopy(renderer, texture, NULL, &rect);
}
