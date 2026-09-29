#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// The dialog owns its icon surfaces; free them with FreeKeyBindingsIcons after dropping it.
std::unique_ptr<ListDialog> MakeKeyBindingsDialog(SFont_Font* font,
                                                  std::function<void()> onBack);

void FreeKeyBindingsIcons();
