#include "core/gamepad.h"
#include "core/log.h"
#include <cstdlib>

static_assert(SDL_CONTROLLER_BUTTON_MAX <= 32, "button bitmask no longer fits");

void Gamepad::Close() {
    if (pad) {
        SDL_GameControllerClose(pad);
        pad = nullptr;
    }
    instanceId = -1;
    buttons = 0;
    prevButtons = 0;
}

void Gamepad::OpenFirst() {
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        pad = SDL_GameControllerOpen(i);
        if (!pad) {
            LOG_WARN("Unable to open game controller %d: %s", i, SDL_GetError());
            continue;
        }
        instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        LOG_INFO("Gamepad connected: %s", Name());
        return;
    }
}

void Gamepad::HandleEvent(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        OpenFirst();
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED &&
               event.cdevice.which == instanceId) {
        LOG_INFO("Gamepad disconnected");
        Close();
        OpenFirst();  // fall back to another pad if one is still attached
    }
}

void Gamepad::Update() {
    prevButtons = buttons;
    buttons = 0;
    if (!pad) return;

    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
        if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b)) {
            buttons |= 1u << b;
        }
    }
}

const char* Gamepad::Name() const {
    if (!pad) return "none";
    const char* n = SDL_GameControllerName(pad);
    return n ? n : "unknown";
}

float Gamepad::Axis(SDL_GameControllerAxis a) const {
    if (!pad) return 0.0f;
    const int v = SDL_GameControllerGetAxis(pad, a);
    if (abs(v) <= DEADZONE) return 0.0f;
    const float scaled = (float)(abs(v) - DEADZONE) / (32767.0f - DEADZONE);
    const float clamped = scaled > 1.0f ? 1.0f : scaled;
    return v < 0 ? -clamped : clamped;
}

bool Gamepad::TriggerDown(SDL_GameControllerAxis a) const {
    return pad && SDL_GameControllerGetAxis(pad, a) > TRIGGER_THRESHOLD;
}
