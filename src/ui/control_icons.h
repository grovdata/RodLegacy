#pragma once

#include <SDL.h>
#include <vector>

// Cells are row-major on a 12-column grid: index = row * 12 + col.
namespace icons {

constexpr int kCellSize = 16;
constexpr int kColumns = 12;

enum class Sheet { Keyboard, Controller };

struct Cell {
    Sheet sheet;
    int index;
};

// `body` replaces the white face, `line` the black lettering and outline. Caller frees the result.
SDL_Surface* Strip(const std::vector<Cell>& cells, SDL_Color body, SDL_Color line);

void Shutdown();

}
