#include "checkbox.h"
#include "rectangle.h"
#include "../state.h"

ButtonConfig Checkbox::CreateButtonConfig(const CheckboxConfig& config) {
    ButtonConfig btnConfig;
    btnConfig.bounds = config.bounds;
    btnConfig.fillColor = config.fillColor;
    btnConfig.borderColor = config.borderColor;
    btnConfig.textColor = {0, 0, 0, 255};
    btnConfig.text = "";
    btnConfig.defaultChoice = false;
    btnConfig.invisible = false;
    btnConfig.index = config.index;
    
    // Create a lambda that toggles the checkbox and calls onChange
    btnConfig.onClick = [this, config]() {
        bool newValue = !(valuePtr ? *valuePtr : localValue);
        
        if (valuePtr) {
            *valuePtr = newValue;
        } else {
            localValue = newValue;
        }
        
        UpdateCheckMark();
        
        if (onChange) {
            onChange(newValue);
        }
    };
    
    return btnConfig;
}

Checkbox::Checkbox(const CheckboxConfig& config) 
    : Button(CreateButtonConfig(config)),
      valuePtr(config.value),
      localValue(false),
      onChange(config.onChange),
      checkColor(config.checkColor),
      bounds(config.bounds)
{
    // Initialize local value if no external pointer provided
    if (!valuePtr) {
        localValue = false;
    }
    UpdateCheckMark();
}

Checkbox::~Checkbox() {
}

void Checkbox::UpdateCheckMark() {
    bool checked = valuePtr ? *valuePtr : localValue;
    
    if (checked) {
        // Create a checkmark (filled square for now)
        int padding = 3;
        
        RectangleConfig checkConfig{};
        checkConfig.rect = {
            bounds.x + padding, 
            bounds.y + padding, 
            bounds.w - (padding * 2), 
            bounds.h - (padding * 2)
        };
        checkConfig.fillColor = checkColor;
        checkConfig.borderColor = checkColor;
        checkConfig.inset = 0;
        
        checkMark = std::make_unique<Rectangle>(checkConfig);
        checkMark->SetVisible(true);
    } else {
        checkMark.reset();
    }
}

void Checkbox::Draw() {
    if (!IsVisible()) return;
    
    // Draw the button background
    Button::Draw();
    
    // Draw the checkmark on top
    if (checkMark) {
        checkMark->Draw();
    }
}

void Checkbox::SetChecked(bool checked) {
    if (valuePtr) {
        *valuePtr = checked;
    } else {
        localValue = checked;
    }
    UpdateCheckMark();
}

bool Checkbox::IsChecked() const {
    return valuePtr ? *valuePtr : localValue;
}
