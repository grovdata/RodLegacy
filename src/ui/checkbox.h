#pragma once

#include <functional>
#include <memory>
#include "../global.h"
#include "button.h"

struct CheckboxConfig {
    SDL_Rect bounds;
    bool* value;  // Pointer to the boolean value to control
    SDL_Color fillColor = {0, 208, 0, 255};
    SDL_Color borderColor = {0, 0, 0, 255};
    SDL_Color checkColor = {0, 0, 0, 255};
    std::function<void(bool)> onChange;  // Called when checkbox value changes
    bool visible = true;
    int index = 0;  // For button compatibility
};

class Checkbox : public Button {
private:
    bool* valuePtr;
    bool localValue;
    std::function<void(bool)> onChange;
    std::unique_ptr<Rectangle> checkMark;
    SDL_Color checkColor;
    SDL_Rect bounds;

    void UpdateCheckMark();
    ButtonConfig CreateButtonConfig(const CheckboxConfig& config);

public:
    Checkbox(const CheckboxConfig& config);
    ~Checkbox();

    void Draw() override;
    void SetChecked(bool checked);
    bool IsChecked() const;
};
