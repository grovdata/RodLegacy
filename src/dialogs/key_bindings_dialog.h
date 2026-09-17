#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// Fullscreen, scrollable list of every control, keyboard and gamepad side by
// side. `onBack` is called when the player leaves it.
//
// The surfaces behind the rows' icon strips belong to the dialog and are freed
// with it, so the caller only owns the dialog itself.
std::unique_ptr<ListDialog> MakeKeyBindingsDialog(SFont_Font* font,
                                                  std::function<void()> onBack);

// Frees the icon strips built for the dialog above. Call after dropping it.
void FreeKeyBindingsIcons();
