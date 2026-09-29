#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// Settings otherwise only in config.txt. Toggling a row writes config.txt immediately.
std::unique_ptr<ListDialog> MakeSettingsDialog(SFont_Font* font,
                                               std::function<void()> onBack);

void FreeSettingsIcons();
