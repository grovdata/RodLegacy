#include "log.h"
#include "bitmap.h"

extern "C" {
#include "lodepng.h"
}


Bitmap::Bitmap() {
    surface.reset();
    texture.reset();
    isVisible = true;
    rect = {0, 0, 0, 0};
    sourceRect = {0, 0, 0, 0};  // Initialize to empty rect
}

Bitmap::Bitmap(int width, int height, int depth) {
    isVisible = true;
    rect = {0, 0, width, height};
    sourceRect = {0, 0, width, height};  // Initialize to full bitmap size
    surface = sdl2::SurfacePtr(SDL_CreateRGBSurface(0, width, height, depth, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
}

Bitmap::Bitmap(const Bitmap& other) {
    // Initialize members
    surface.reset();
    texture.reset();
    isVisible = other.isVisible;
    rect = other.rect;
    sourceRect = other.sourceRect;

    // Deep-copy the surface so the copy owns its own pixel buffer.
    // SDL_CreateRGBSurfaceFrom only borrows the source pixels and must NOT
    // be used here — the source may be destroyed before the copy.
    if (other.surface) {
        surface = sdl2::SurfacePtr(SDL_ConvertSurface(other.surface.get(), other.surface->format, 0));
        if (surface) {
            texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
        }
    }
}

Bitmap::~Bitmap() {
    // RAII handles cleanup automatically
}

 // Assignment from pointer
Bitmap& Bitmap::operator=(const Bitmap* other) {
    if (other) {
        surface.reset();
        texture.reset();

        if (other->surface) {
            surface = sdl2::SurfacePtr(SDL_ConvertSurface(other->surface.get(), other->surface->format, 0));
            if (surface) {
                texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
            }
        }
        isVisible = other->isVisible;
        rect = other->rect;
        sourceRect = other->sourceRect;
    }
    return *this;
}

// Regular assignment operator
Bitmap& Bitmap::operator=(const Bitmap& other) {
    if (this != &other) {
        surface.reset();
        texture.reset();

        if (other.surface) {
            surface = sdl2::SurfacePtr(SDL_ConvertSurface(other.surface.get(), other.surface->format, 0));
            if (surface) {
                texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
            }
        }
        isVisible = other.isVisible;
        rect = other.rect;
        sourceRect = other.sourceRect;
    }
    return *this;
}

SDL_Surface* Bitmap::GetSurface() {
    return surface.get();
}

SDL_Texture* Bitmap::GetTexture() {
    return texture.get();
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
    sdl2::SurfacePtr cropped(SDL_CreateRGBSurface(
        0,
        crop.w,
        crop.h,
        surface->format->BitsPerPixel,
        surface->format->Rmask,
        surface->format->Gmask,
        surface->format->Bmask,
        surface->format->Amask
    ));

    if (!cropped) {
        LOG_ERROR("SDL_CreateRGBSurface failed: %s", SDL_GetError());
        return;
    }

    // Copy the pixels from the original surface to the new cropped surface
    SDL_BlitSurface(surface.get(), &crop, cropped.get(), nullptr);
    
    // Replace surface and texture
    surface = std::move(cropped);
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
}

void Bitmap::CreateMask(SDL_Surface *mask, SDL_Surface *target, SDL_Rect rect) {
    // Note: mask and target are external surfaces, we don't own them
    SrLibColor maskColor = AmigaMaskColor;
    uint8_t maskR = maskColor.r;
    uint8_t maskG = maskColor.g;
    uint8_t maskB = maskColor.b;

    LOG_INFO("Looking for mask color RGB(%d,%d,%d)", maskR, maskG, maskB);
    LOG_INFO("First pixel comparison: Mask RGB(128,16,0) == Target RGB(%d,%d,%d): %s",
            maskR, maskG, maskB,
            (128 == maskR && 16 == maskG && 0 == maskB) ? "true" : "false");

    if (target->format != surface->format) {
        LOG_WARN("Target surface format differs from source format - this might cause issues");
        LOG_WARN("Source BPP: %d, Target BPP: %d", surface->format->BitsPerPixel, target->format->BitsPerPixel);
    }

    // Ensure the rect does not exceed the bounds of the surfaces
    if (rect.x < 0 || rect.y < 0 ||
        rect.x + rect.w > surface->w || rect.y + rect.h > surface->h) {
        LOG_ERROR("Rectangle exceeds the bounds of the surface surface!\n");
        return;
    }

    if (rect.w > mask->w || rect.h > mask->h) {
        LOG_ERROR("Mask dimensions are smaller than the rectangle area!\n");
        return;
    }

    // Lock surfaces for pixel access if necessary
    if (SDL_MUSTLOCK(surface.get())) SDL_LockSurface(surface.get());
    if (SDL_MUSTLOCK(mask)) SDL_LockSurface(mask);
    if (SDL_MUSTLOCK(target)) SDL_LockSurface(target);

    // Access pixel data
    Uint32 *bgPixels = (Uint32 *)surface->pixels;
    Uint32 *maskPixels = (Uint32 *)mask->pixels;
    Uint32 *targetPixels = (Uint32 *)target->pixels;

    SDL_PixelFormat *maskFormat = mask->format;

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
            } else {
                targetPixels[targetIdx] = SDL_MapRGBA(target->format, 0, 0, 0, 0);
            }
        }
    }

    // Unlock surfaces
    if (SDL_MUSTLOCK(surface.get())) SDL_UnlockSurface(surface.get());
    if (SDL_MUSTLOCK(mask)) SDL_UnlockSurface(mask);
    if (SDL_MUSTLOCK(target)) SDL_UnlockSurface(target);
}


void Bitmap::SetVisible(bool visible) {
    this->isVisible = visible;
}

void Bitmap::LoadBitmap(std::string filename) {
    surface = sdl2::SurfacePtr(IMG_Load(filename.c_str()));
    //SDL_SetColorKey(surface.get(), 0, SDL_MapRGB(surface->format, 0, 0, 0));
    if (!surface) {
        LOG_INFO("Bitmap::LoadBitmap error, could not load surface %s\n", filename.c_str());
        return;
    } else {
        LOG_DEBUG("Loaded bitmap %s, size %d x %d\n", filename.c_str(), surface->w, surface->h);
    }
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;
    sourceRect = {0, 0, surface->w, surface->h};  // Initialize to full bitmap size
}

void Bitmap::LoadSurface(SDL_Surface* src) {
    if (src == nullptr) {
        LOG_ERROR("Source surface is null!");
        return;
    }

    // Create a new surface with the same format and size as the source
    surface = sdl2::SurfacePtr(SDL_CreateRGBSurfaceWithFormat(
        0, src->w, src->h, src->format->BitsPerPixel, src->format->format));

    if (!surface) {
        LOG_ERROR("Could not create surface: %s", SDL_GetError());
        return;
    }

    // Copy the pixel data
    if (SDL_BlitSurface(src, nullptr, surface.get(), nullptr) < 0) {
        LOG_ERROR("Could not copy surface: %s", SDL_GetError());
        return;
    }

    rect.x = 0;
    rect.y = 0;
    rect.w = surface->w;
    rect.h = surface->h;
    sourceRect = {0, 0, surface->w, surface->h};  // Initialize to full bitmap size

    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
    if (!texture) {
        LOG_ERROR("Could not create SDL texture: %s", SDL_GetError());
    }
}

void Bitmap::LoadBitmapFromChunky(const std::vector<SrLibColor>& colors, int width, int height, bool isMask, bool opaque) {
    if (int(colors.size()) != width * height) {
        LOG_ERROR("The size of the color vector does not match the specified width and height.");
        return;
    }

    // Create an SDL surface with the specified width and height
    surface = sdl2::SurfacePtr(SDL_CreateRGBSurface(0, width, height, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF));
    if (!surface) {
        LOG_ERROR("Could not create SDL surface: %s", SDL_GetError());
        return;
    }

    SDL_LockSurface(surface.get());
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);

    for (int i = 0; i < width * height; ++i) {
        uint8_t r = colors[i].r;
        uint8_t g = colors[i].g;
        uint8_t b = colors[i].b;

        uint8_t a;
        if (isMask) {
            // For masks: preserve original colors, make fully opaque
            a = 255;
        } else {
            // Normal bitmap behavior: first pixel transparent, rest opaque
            if (opaque) {
                a = 255;
            } else {
                a = (r == 0 && g == 0 && b == 0) ? 0 : 255;
            }
        }

        pixels[i] = SDL_MapRGBA(surface->format, r, g, b, a);
    }

    SDL_UnlockSurface(surface.get());

    // Set up alpha blending for the texture
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
    if (!texture) {
        LOG_ERROR("Could not create SDL texture: %s", SDL_GetError());
    } else {
        SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
    }

    sourceRect = {0, 0, width, height};  // Initialize to full bitmap size
}

void Bitmap::SaveBitmap(int index) {
    char filename[30];
    snprintf(filename, sizeof(filename), "sprite_%d.png", index);

    // Get surface pixels and convert to RGBA format
    sdl2::SurfacePtr rgbaSurface(SDL_ConvertSurfaceFormat(surface.get(), SDL_PIXELFORMAT_RGBA32, 0));
    if (!rgbaSurface) {
        LOG_ERROR("Failed to convert surface: %s", SDL_GetError());
        return;
    }

    // Get pixel data
    const size_t pixelCount = rgbaSurface->w * rgbaSurface->h * 4;
    uint8_t* image = new uint8_t[pixelCount];

    SDL_LockSurface(rgbaSurface.get());
    memcpy(image, rgbaSurface->pixels, pixelCount);
    SDL_UnlockSurface(rgbaSurface.get());

    // Encode and save using lodepng
    unsigned error = lodepng_encode32_file(filename, image, rgbaSurface->w, rgbaSurface->h);
    if (error) {
        LOG_ERROR("Failed to save PNG file: %s", lodepng_error_text(error));
    }

    delete[] image;
    // rgbaSurface automatically freed by RAII
}

void Bitmap::SetSourceRect(const SDL_Rect& rect) {
    sourceRect = rect;
}

void Bitmap::Draw() {
    if (isVisible && texture) {
        if (sourceRect.w > 0 && sourceRect.h > 0) {
            SDL_RenderCopy(renderer, texture.get(), &sourceRect, &rect);
        } else {
            SDL_RenderCopy(renderer, texture.get(), NULL, &rect);
        }
    }
}

void Bitmap::DrawWithRects(const SDL_Rect& src, const SDL_Rect& dst) {
    if (texture && src.w > 0 && src.h > 0) {
        SDL_RenderCopy(renderer, texture.get(), &src, &dst);
    }
}

void Bitmap::UpdateFromSurface() {
    texture.reset();  // RAII handles cleanup
    texture = sdl2::TexturePtr(SDL_CreateTextureFromSurface(renderer, surface.get()));
    SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
}

void Bitmap::ReplaceSurface(SDL_Surface* newSurface) {
    // Note: This function takes ownership of newSurface
    // If we already have a surface and it's different, reset it
    if (surface && surface.get() != newSurface) {
        surface.reset();
    }
    // Take ownership of the new surface
    surface = sdl2::SurfacePtr(newSurface);
    UpdateFromSurface();
}

void Bitmap::ApplyBumperOverlay(Bitmap* overlay, int offsetX, int offsetY) {
    if (!surface || !overlay || !overlay->surface) {
        LOG_INFO("ApplyBumperOverlay: Invalid surfaces");
        return;
    }

    SDL_Surface* overlaySurface = overlay->surface.get();

    // Lock surfaces for pixel manipulation
    if (SDL_MUSTLOCK(surface.get())) SDL_LockSurface(surface.get());
    if (SDL_MUSTLOCK(overlaySurface)) SDL_LockSurface(overlaySurface);

    uint32_t* bodyPixels = static_cast<uint32_t*>(surface->pixels);
    uint32_t* overlayPixels = static_cast<uint32_t*>(overlaySurface->pixels);

    // Apply overlay: black pixels make body transparent, colored pixels replace body
    for (int y = 0; y < overlaySurface->h && (y + offsetY) < surface->h; y++) {
        for (int x = 0; x < overlaySurface->w && (x + offsetX) < surface->w; x++) {
            if (offsetX + x < 0 || offsetY + y < 0) continue;

            int overlayIdx = y * overlaySurface->w + x;
            int bodyIdx = (y + offsetY) * surface->w + (x + offsetX);

            uint32_t overlayPixel = overlayPixels[overlayIdx];
            uint8_t r, g, b, a;
            SDL_GetRGBA(overlayPixel, overlaySurface->format, &r, &g, &b, &a);

            // If overlay pixel is transparent (black), make body pixel transparent
            if (a == 0 || (r == 0 && g == 0 && b == 0)) {
                // Make this pixel transparent (punch through to background)
                bodyPixels[bodyIdx] = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
            } else if (a > 0) {
                // Overlay has a colored pixel - this is the "no bumper" body part
                bodyPixels[bodyIdx] = SDL_MapRGBA(surface->format, r, g, b, a);
            }
        }
    }

    // Unlock surfaces
    if (SDL_MUSTLOCK(surface.get())) SDL_UnlockSurface(surface.get());
    if (SDL_MUSTLOCK(overlaySurface)) SDL_UnlockSurface(overlaySurface);

    // Recreate texture from modified surface
    UpdateFromSurface();
    LOG_INFO("Applied bumper overlay at offset (%d, %d)", offsetX, offsetY);
}
