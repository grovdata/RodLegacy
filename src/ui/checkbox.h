/*
 * RodLegacy - A checkbox button: Checkbox.
 *
 * Created: 2025-10-15
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <functional>
#include <memory>
#include "core/global.h"
#include "ui/button.h"

struct CheckboxConfig {
    SDL_Rect bounds;
    bool* value;
    std::function<void(bool)> onChange;
    bool visible = true;
    int index = 0;
};

class Checkbox : public Button {
private:
    bool* valuePtr;
    bool localValue;
    std::function<void(bool)> onChange;
    std::unique_ptr<Bitmap> icon;
    SDL_Rect bounds;

    void UpdateIcon();
    ButtonConfig CreateButtonConfig(const CheckboxConfig& config);

public:
    Checkbox(const CheckboxConfig& config);
    ~Checkbox();

    void Draw() override;
    void SetChecked(bool checked);
    bool IsChecked() const;
};
