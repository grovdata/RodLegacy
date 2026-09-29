#include "ui/checkbox.h"
#include "gfx/bitmap.h"
#include "core/embedded_loader.h"
#include "core/state.h"

extern "C" {
    extern const unsigned char embedded_checkbox_on_png[];
    extern const size_t embedded_checkbox_on_png_len;
    extern const unsigned char embedded_checkbox_off_png[];
    extern const size_t embedded_checkbox_off_png_len;
}

ButtonConfig Checkbox::CreateButtonConfig(const CheckboxConfig& config) {
    ButtonConfig btnConfig;
    btnConfig.bounds = config.bounds;
    btnConfig.text = "";
    btnConfig.defaultChoice = false;
    btnConfig.invisible = true;
    btnConfig.index = config.index;

    btnConfig.onClick = [this, config]() {
        bool newValue = !(valuePtr ? *valuePtr : localValue);

        if (valuePtr) {
            *valuePtr = newValue;
        } else {
            localValue = newValue;
        }

        UpdateIcon();

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
      bounds(config.bounds)
{
    if (!valuePtr) {
        localValue = false;
    }
    UpdateIcon();
}

Checkbox::~Checkbox() {
}

void Checkbox::UpdateIcon() {
    bool checked = valuePtr ? *valuePtr : localValue;

    const unsigned char* data = checked ? embedded_checkbox_on_png : embedded_checkbox_off_png;
    size_t len = checked ? embedded_checkbox_on_png_len : embedded_checkbox_off_png_len;

    SDL_Surface* iconSurface = LoadSurfaceFromMemory(data, len);
    if (!iconSurface) return;

    icon = std::make_unique<Bitmap>();
    icon->LoadSurface(iconSurface);
    SDL_FreeSurface(iconSurface);
    icon->SetPosition(bounds.x, bounds.y);
    icon->SetDimensions(bounds.w, bounds.h);
    icon->SetVisible(true);
}

void Checkbox::Draw() {
    if (!IsVisible()) return;
    if (icon) icon->Draw();
}

void Checkbox::SetChecked(bool checked) {
    if (valuePtr) {
        *valuePtr = checked;
    } else {
        localValue = checked;
    }
    UpdateIcon();
}

bool Checkbox::IsChecked() const {
    return valuePtr ? *valuePtr : localValue;
}
