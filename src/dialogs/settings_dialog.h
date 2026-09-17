#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// Fullscreen, scrollable list of the settings that have no other UI - the ones
// otherwise only reachable by hand-editing config.txt. Same style as the key
// bindings screen. Toggling a row writes config.txt straight away.
//
// `onBack` is called when the player leaves it.
std::unique_ptr<ListDialog> MakeSettingsDialog(SFont_Font* font,
                                               std::function<void()> onBack);

// Frees the checkbox art built for the dialog above. Call after dropping it.
void FreeSettingsIcons();
