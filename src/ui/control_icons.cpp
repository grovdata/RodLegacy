#include "ui/control_icons.h"

#include "core/embedded_loader.h"
#include "core/log.h"

extern "C" {
    #include "embedded_resources.h"
}

namespace icons {
namespace {

SDL_Surface* keyboardSheet = nullptr;
SDL_Surface* controllerSheet = nullptr;
bool loadAttempted = false;

void LoadSheets() {
    if (loadAttempted) return;
    loadAttempted = true;

    keyboardSheet = LoadSurfaceFromMemory(embedded_icons_keyboard_16x16_1bit_ansdor_png,
                                          embedded_icons_keyboard_16x16_1bit_ansdor_png_len);
    controllerSheet = LoadSurfaceFromMemory(embedded_icons_controller_16x16_1bit_ansdor_png,
                                            embedded_icons_controller_16x16_1bit_ansdor_png_len);
    if (!keyboardSheet || !controllerSheet) {
        LOG_WARN("control_icons: failed to decode an icon sheet");
    }
}

SDL_Surface* SheetFor(Sheet s) {
    LoadSheets();
    return (s == Sheet::Keyboard) ? keyboardSheet : controllerSheet;
}

SDL_Rect CellRect(int index) {
    return {(index % kColumns) * kCellSize, (index / kColumns) * kCellSize,
            kCellSize, kCellSize};
}

// Each icon is exactly three values: white (the cap or button body), black (its
// lettering and outline) and transparent (outside the rounded corners). A
// colour mod only multiplies, so it can never lift the black off zero -
// remapping the two inks independently has to happen per pixel.
//
// Repaints a blitted strip: every opaque pixel becomes either `body` or `line`
// depending on which of the sheet's two inks it came from.
void Recolour(SDL_Surface* s, SDL_Color body, SDL_Color line) {
    if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) != 0) {
        LOG_WARN("control_icons: could not lock a strip: %s", SDL_GetError());
        return;
    }

    const Uint32 bodyPx = SDL_MapRGBA(s->format, body.r, body.g, body.b, 0xFF);
    const Uint32 linePx = SDL_MapRGBA(s->format, line.r, line.g, line.b, 0xFF);
    const int stride = s->pitch / 4;
    Uint32* pixels = static_cast<Uint32*>(s->pixels);

    for (int y = 0; y < s->h; ++y) {
        for (int x = 0; x < s->w; ++x) {
            Uint32& p = pixels[y * stride + x];
            Uint8 r, g, b, a;
            SDL_GetRGBA(p, s->format, &r, &g, &b, &a);
            if (a == 0) continue;
            // The sheets hold only white and black ink, so one threshold
            // separates the body from the line art.
            p = (r > 127) ? bodyPx : linePx;
        }
    }

    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
}

}  // namespace

SDL_Surface* Strip(const std::vector<Cell>& cells, SDL_Color body, SDL_Color line) {
    if (cells.empty()) return nullptr;

    const int w = static_cast<int>(cells.size()) * kCellSize;
    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, w, kCellSize, 32,
                                                      SDL_PIXELFORMAT_RGBA32);
    if (!out) {
        LOG_WARN("control_icons: could not make a %dx%d strip: %s", w, kCellSize,
                 SDL_GetError());
        return nullptr;
    }
    SDL_FillRect(out, nullptr, SDL_MapRGBA(out->format, 0, 0, 0, 0));

    bool drewAnything = false;
    for (size_t i = 0; i < cells.size(); ++i) {
        // A negative index is a deliberate gap: it advances a cell and draws
        // nothing, which is how a strip separates its keyboard and pad halves.
        if (cells[i].index < 0) continue;

        SDL_Surface* sheet = SheetFor(cells[i].sheet);
        if (!sheet) continue;

        SDL_Rect src = CellRect(cells[i].index);
        if (src.x + src.w > sheet->w || src.y + src.h > sheet->h) {
            LOG_WARN("control_icons: cell %d is off the sheet", cells[i].index);
            continue;
        }

        // Straight copy, no blending: the two inks have to arrive unmodified so
        // Recolour can tell them apart.
        SDL_SetSurfaceBlendMode(sheet, SDL_BLENDMODE_NONE);
        SDL_Rect dst = {static_cast<int>(i) * kCellSize, 0, kCellSize, kCellSize};
        SDL_BlitSurface(sheet, &src, out, &dst);
        drewAnything = true;
    }

    if (!drewAnything) {
        SDL_FreeSurface(out);
        return nullptr;
    }

    Recolour(out, body, line);
    return out;
}

void Shutdown() {
    if (keyboardSheet) {
        SDL_FreeSurface(keyboardSheet);
        keyboardSheet = nullptr;
    }
    if (controllerSheet) {
        SDL_FreeSurface(controllerSheet);
        controllerSheet = nullptr;
    }
    loadAttempted = false;
}

}  // namespace icons
