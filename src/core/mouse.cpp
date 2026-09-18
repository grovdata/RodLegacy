#include "core/log.h"
#include "core/mouse.h"
#include "core/zone_nav.h"
#include "dialogs/dialog.h"
#include "scenes/scene.h"
#include <algorithm>

namespace {
// A stick this far over counts as "the player reached for the pad".
constexpr Sint16 ACTIVATE_AXIS = 12000;
// Cursor speed in pixels per second at full stick deflection.
constexpr float CURSOR_SPEED = 160.0f;

std::vector<nav::Rect> ToNavRects(const std::vector<SDL_Rect>& in) {
    std::vector<nav::Rect> out;
    out.reserve(in.size());
    for (const SDL_Rect& r : in) out.push_back({r.x, r.y, r.w, r.h});
    return out;
}
}  // namespace

void Mouse::Update(SDL_Event *event, Scene*) {
    if (event->type == SDL_MOUSEMOTION) {
        rect.x = event->motion.x;
        rect.y = event->motion.y;
        // If mouse moves, deactivate gamepad mode
        gamepadActive = false;
    }

    // Reaching for the pad switches back to gamepad mode. Axis events fire on
    // resting-stick jitter too, so they only count past a generous deadzone.
    if (event->type == SDL_CONTROLLERAXISMOTION &&
        abs(event->caxis.value) > ACTIVATE_AXIS) {
        gamepadActive = true;
    }

    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        gamepadActive = true;
    }
}

std::vector<SDL_Rect> Mouse::NavTargets(Scene* scene) const {
    std::vector<SDL_Rect> out;
    if (!scene) return out;

    // A dialog is modal: its buttons are the only thing worth jumping to, and
    // the scene zones underneath are not clickable while it is up. A dialog
    // with nothing to aim at falls through to the scene rather than trapping
    // the cursor.
    if (Dialog* dialog = scene->GetActiveDialog()) {
        if (dialog->IsVisible()) {
            std::vector<SDL_Rect> targets = dialog->NavTargets();
            if (!targets.empty()) return targets;
        }
    }

    for (const Zone& z : scene->GetZones()) {
        if (z.IsEnabled()) out.push_back(z.GetRect());
    }
    return out;
}

// The hotspot is the top-left pixel of the sprite, as it is for a real mouse
// (SDL_MOUSEMOTION sets rect.x/y straight from the event), so snapping puts
// that pixel on the target centre and the sprite hangs down-right of it.
void Mouse::MoveCursorTo(const SDL_Rect& target) {
    rect.x = std::max(0, std::min(target.x + target.w / 2, GAME_WIDTH - 1));
    rect.y = std::max(0, std::min(target.y + target.h / 2, GAME_HEIGHT - 1));
    subPixelX = 0.0f;
    subPixelY = 0.0f;
}

void Mouse::SendClick(Scene* scene, Uint8 button) {
    if (!scene) return;
    SDL_Event e;
    SDL_zero(e);
    e.button.type = SDL_MOUSEBUTTONDOWN;
    e.button.timestamp = SDL_GetTicks();
    e.button.button = button;
    e.button.clicks = 1;
    e.button.state = SDL_PRESSED;
    e.button.x = rect.x;
    e.button.y = rect.y;
    scene->PushEvent(&e);

    e.button.type = SDL_MOUSEBUTTONUP;
    e.button.state = SDL_RELEASED;
    scene->PushEvent(&e);
}

void Mouse::SendMotion(Scene* scene) {
    if (!scene) return;
    SDL_Event e;
    SDL_zero(e);
    e.motion.type = SDL_MOUSEMOTION;
    e.motion.timestamp = SDL_GetTicks();
    e.motion.x = rect.x;
    e.motion.y = rect.y;
    scene->PushEvent(&e);
}

void Mouse::SendKey(Scene* scene, Uint32 type, SDL_Keycode key) {
    if (!scene) return;
    SDL_Event e;
    SDL_zero(e);
    e.key.type = type;
    e.key.timestamp = SDL_GetTicks();
    e.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
    e.key.keysym.sym = key;
    e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    scene->PushEvent(&e);
}

void Mouse::UpdateGamepad(Scene* scene, Uint64 deltaMs) {
    gamepadDebug.connected = gamepad.Connected();
    if (!gamepad.Connected() || !gamepadActive) {
        return;
    }

    // The driving scene reads the pad itself (throttle, brake, steering); a
    // cursor there would only fight it.
    if (scene && !scene->WantsPointer()) {
        return;
    }

    const Uint32 currentTime = SDL_GetTicks();

    DPadDirection currentDirection = DPadDirection::NONE;
    if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_UP)) {
        currentDirection = DPadDirection::UP;
        gamepadDebug.lastAction = "D-pad UP";
    } else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
        currentDirection = DPadDirection::DOWN;
        gamepadDebug.lastAction = "D-pad DOWN";
    } else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
        currentDirection = DPadDirection::LEFT;
        gamepadDebug.lastAction = "D-pad LEFT";
    } else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
        currentDirection = DPadDirection::RIGHT;
        gamepadDebug.lastAction = "D-pad RIGHT";
    }

    // Move on the press, then on a repeat delay while it is held.
    if (currentDirection != DPadDirection::NONE) {
        if (currentDirection != lastDPadDirection ||
            (currentTime - lastDPadTime) > DPAD_REPEAT_DELAY) {
            JumpToClosestZoneInDirection(scene, currentDirection);
            lastDPadTime = currentTime;
        }
    }
    lastDPadDirection = currentDirection;

    // Left stick moves the cursor freely, in pixels per second so the speed
    // does not follow the frame rate.
    const float leftX = gamepad.Axis(SDL_CONTROLLER_AXIS_LEFTX);
    const float leftY = gamepad.Axis(SDL_CONTROLLER_AXIS_LEFTY);
    gamepadDebug.leftX = leftX;
    gamepadDebug.leftY = leftY;

    if (leftX != 0.0f || leftY != 0.0f) {
        const float seconds = (float)deltaMs / 1000.0f;
        subPixelX += leftX * CURSOR_SPEED * seconds;
        subPixelY += leftY * CURSOR_SPEED * seconds;

        const int stepX = (int)subPixelX;
        const int stepY = (int)subPixelY;
        subPixelX -= (float)stepX;
        subPixelY -= (float)stepY;
        rect.x += stepX;
        rect.y += stepY;

        gamepadDebug.lastAction = "Analog " + std::to_string((int)(leftX * 100)) +
                                  "," + std::to_string((int)(leftY * 100));
    } else {
        subPixelX = 0.0f;
        subPixelY = 0.0f;
    }

    rect.x = std::max(0, std::min(rect.x, GAME_WIDTH - 1));
    rect.y = std::max(0, std::min(rect.y, GAME_HEIGHT - 1));

    // Zones highlight, and the HUD names what is under the cursor, off motion
    // events. Moving the cursor by hand produces none, so the pad has to say it
    // moved or the scene never notices - once per actual move, not per frame.
    if (rect.x != lastMotionX || rect.y != lastMotionY) {
        lastMotionX = rect.x;
        lastMotionY = rect.y;
        SendMotion(scene);
    }

    gamepadDebug.buttonA = gamepad.Down(SDL_CONTROLLER_BUTTON_A);
    gamepadDebug.buttonB = gamepad.Down(SDL_CONTROLLER_BUTTON_B);
    gamepadDebug.buttonX = gamepad.Down(SDL_CONTROLLER_BUTTON_X);
    gamepadDebug.buttonY = gamepad.Down(SDL_CONTROLLER_BUTTON_Y);
    gamepadDebug.buttonLB = gamepad.Down(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    gamepadDebug.buttonRB = gamepad.Down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

    // Everything below acts on the press, not on the hold, or one tap would
    // fire once per frame for as long as the button is down.
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_A)) {
        gamepadDebug.lastAction = "A (click)";
        SendClick(scene, SDL_BUTTON_LEFT);
    }

    // B is back. Not a right click: scene zones fire onClick for any mouse
    // button, so a right click there is indistinguishable from A.
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_B)) {
        gamepadDebug.lastAction = "B (back)";
        SendKey(scene, SDL_KEYDOWN, SDLK_ESCAPE);
        SendKey(scene, SDL_KEYUP, SDLK_ESCAPE);
    }

    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_X)) {
        gamepadDebug.lastAction = "X (snap to zone)";
        JumpToClosestZone(scene);
    }

    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
        gamepadDebug.lastAction = "LB (garage)";
        SendKey(scene, SDL_KEYUP, SDLK_g);
    }

    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        gamepadDebug.lastAction = "RB (diner)";
        SendKey(scene, SDL_KEYUP, SDLK_d);
    }

    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_START)) {
        gamepadDebug.lastAction = "Start (enter)";
        SendKey(scene, SDL_KEYDOWN, SDLK_RETURN);
        SendKey(scene, SDL_KEYUP, SDLK_RETURN);
    }
}

std::vector<DebugRow> Mouse::GetGamepadDebugRows() const {
    std::vector<DebugRow> rows;
    rows.push_back({"connected", gamepadDebug.connected ? gamepad.Name() : "no"});
    rows.push_back({"active", gamepadActive ? "yes" : "no"});
    rows.push_back({"mouse", std::to_string(rect.x) + ", " + std::to_string(rect.y)});

    if (gamepadDebug.connected) {
        rows.push_back({"left stick",
                        "X=" + std::to_string((int)(gamepadDebug.leftX * 100)) +
                        "% Y=" + std::to_string((int)(gamepadDebug.leftY * 100)) + "%"});

        std::string buttons;
        if (gamepadDebug.buttonA) buttons += "A ";
        if (gamepadDebug.buttonB) buttons += "B ";
        if (gamepadDebug.buttonX) buttons += "X ";
        if (gamepadDebug.buttonY) buttons += "Y ";
        if (gamepadDebug.buttonLB) buttons += "LB ";
        if (gamepadDebug.buttonRB) buttons += "RB ";
        rows.push_back({"buttons", buttons.empty() ? "none" : buttons});

        if (!gamepadDebug.lastAction.empty()) {
            rows.push_back({"last action", gamepadDebug.lastAction});
        }
    }
    return rows;
}

std::vector<std::string> Mouse::GetGamepadDebugLines() const {
    std::vector<std::string> lines;
    lines.push_back("=== GAMEPAD DEBUG ===");
    for (const DebugRow& row : GetGamepadDebugRows()) {
        lines.push_back(row.key + ": " + row.value);
    }
    return lines;
}

void Mouse::JumpToClosestZoneInDirection(Scene* scene, DPadDirection direction) {
    const std::vector<SDL_Rect> targets = NavTargets(scene);
    if (targets.empty()) return;
    const std::vector<nav::Rect> rects = ToNavRects(targets);

    nav::Dir dir;
    switch (direction) {
        case DPadDirection::UP:    dir = nav::Dir::Up; break;
        case DPadDirection::DOWN:  dir = nav::Dir::Down; break;
        case DPadDirection::LEFT:  dir = nav::Dir::Left; break;
        case DPadDirection::RIGHT: dir = nav::Dir::Right; break;
        default: return;
    }

    const int currentX = rect.x;
    const int currentY = rect.y;

    const int pick = nav::PickDirectional(rects, currentX, currentY, dir);
    if (pick < 0) {
        // Nothing that way: stay put rather than teleporting somewhere
        // unrelated, so holding a direction settles on the last target.
        return;
    }
    MoveCursorTo(targets[pick]);
}

void Mouse::JumpToClosestZone(Scene* scene) {
    const std::vector<SDL_Rect> targets = NavTargets(scene);
    if (targets.empty()) {
        rect.x = GAME_WIDTH / 2;
        rect.y = GAME_HEIGHT / 2;
        return;
    }

    const std::vector<nav::Rect> rects = ToNavRects(targets);
    const int pick = nav::PickNearest(rects, rect.x, rect.y);
    if (pick < 0) return;
    MoveCursorTo(targets[pick]);
}
