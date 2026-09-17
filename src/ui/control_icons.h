#pragma once

#include <SDL.h>
#include <vector>

// The two 16x16 1-bit icon sheets (Ansdor). Cells are indexed row-major across
// a 12-column grid, so index = row * 12 + col.
namespace icons {

constexpr int kCellSize = 16;
constexpr int kColumns = 12;

enum class Sheet { Keyboard, Controller };

struct Cell {
    Sheet sheet;
    int index;
};

// One row of icons drawn left to right on a fresh transparent surface.
//
// Each icon is drawn in exactly two colours: `body` replaces the white cap or
// button face, `line` replaces the black lettering and outline. The corners
// outside the rounded shape stay transparent. Returns null if nothing could be
// drawn; the caller owns the result and frees it with SDL_FreeSurface.
SDL_Surface* Strip(const std::vector<Cell>& cells, SDL_Color body, SDL_Color line);

// Frees the decoded sheets. Safe to call without ever having drawn anything.
void Shutdown();

}  // namespace icons
