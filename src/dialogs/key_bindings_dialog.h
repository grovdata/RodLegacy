/*
 * RodLegacy - The key bindings screen: every control with its keyboard and gamepad icons.
 *
 * Created: 2026-09-17
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// The dialog owns its icon surfaces; free them with FreeKeyBindingsIcons after dropping it.
std::unique_ptr<ListDialog> MakeKeyBindingsDialog(SFont_Font* font,
                                                  std::function<void()> onBack);

void FreeKeyBindingsIcons();
