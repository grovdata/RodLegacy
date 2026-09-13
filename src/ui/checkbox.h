#pragma once

#include <functional>
#include <memory>
#include "core/global.h"
#include "ui/button.h"

struct CheckboxConfig {
    SDL_Rect bounds;
    bool* value;  // Pointer to the boolean value to control
    std::function<void(bool)> onChange;  // Called when checkbox value changes
    bool visible = true;
    int index = 0;  // For button compatibility
};

// Rendered entirely from checkbox-on.png / checkbox-off.png (see UpdateIcon) -
// the button base class contributes only click handling, not visuals.
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
