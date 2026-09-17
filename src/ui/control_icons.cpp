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

// The sheets are 1-bit art: every lit pixel is the same colour. Recolouring is
// therefore just a colour mod, applied per blit rather than by editing pixels.
SDL_Rect CellRect(int index) {
    return {(index % kColumns) * kCellSize, (index / kColumns) * kCellSize,
            kCellSize, kCellSize};
}

}  // namespace

SDL_Surface* Strip(const std::vector<Cell>& cells, SDL_Color colour) {
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

        SDL_SetSurfaceColorMod(sheet, colour.r, colour.g, colour.b);
        SDL_SetSurfaceBlendMode(sheet, SDL_BLENDMODE_BLEND);
        SDL_Rect dst = {static_cast<int>(i) * kCellSize, 0, kCellSize, kCellSize};
        SDL_BlitSurface(sheet, &src, out, &dst);
        drewAnything = true;
    }

    if (!drewAnything) {
        SDL_FreeSurface(out);
        return nullptr;
    }
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
