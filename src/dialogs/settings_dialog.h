/*
 * RodLegacy - The settings screen: toggles for options with no other UI, written to config.txt.
 *
 * Created: 2026-09-17
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "dialogs/list_dialog.h"
#include "gfx/SFont.h"

#include <functional>
#include <memory>

// Settings otherwise only in config.txt. Toggling a row writes config.txt immediately.
std::unique_ptr<ListDialog> MakeSettingsDialog(SFont_Font* font,
                                               std::function<void()> onBack);

void FreeSettingsIcons();
