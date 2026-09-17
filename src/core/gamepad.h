#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <SDL.h>

// One game controller, via SDL's mapped GameController API so button names mean
// the same thing on every pad. Snapshot once per frame with Update(); Pressed()
// is then the edge and Down() the level.
class Gamepad {
public:
    ~Gamepad() { Close(); }

    // Opens the first attached pad. Safe to call before any is plugged in.
    void OpenFirst();
    // Feed CONTROLLERDEVICEADDED / CONTROLLERDEVICEREMOVED for hotplug.
    void HandleEvent(const SDL_Event& event);
    void Update();

    bool Connected() const { return pad != nullptr; }
    const char* Name() const;

    bool Down(SDL_GameControllerButton b) const { return (buttons >> b) & 1u; }
    bool Pressed(SDL_GameControllerButton b) const {
        return ((buttons & ~prevButtons) >> b) & 1u;
    }

    // -1..1 with the deadzone removed and the remainder rescaled.
    float Axis(SDL_GameControllerAxis a) const;
    // Triggers read 0..32767, so they need their own threshold.
    bool TriggerDown(SDL_GameControllerAxis a) const;

    static constexpr Sint16 DEADZONE = 8000;
    static constexpr Sint16 TRIGGER_THRESHOLD = 8000;

private:
    void Close();

    SDL_GameController* pad = nullptr;
    SDL_JoystickID instanceId = -1;
    Uint32 buttons = 0;
    Uint32 prevButtons = 0;
};

extern Gamepad gamepad;

#endif
