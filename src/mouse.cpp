#include "log.h"
#include "mouse.h"
#include "scene.h"
#include <cmath>
#include <algorithm>

void Mouse::Update(SDL_Event *event, Scene*) {
    if (event->type == SDL_MOUSEMOTION) {
        rect.x = event->motion.x;
        rect.y = event->motion.y;
        // If mouse moves, deactivate gamepad mode
        gamepadActive = false;
    }

    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_RIGHT) {
        switch (pointerType) {
            case SrLibAmigaSpriteAssetMousePointerHand:
                pointerType = SrLibAmigaSpriteAssetMousePointerWrench;
                break;
            case SrLibAmigaSpriteAssetMousePointerWrench:
                pointerType = SrLibAmigaSpriteAssetMousePointerPump;
                break;
            case SrLibAmigaSpriteAssetMousePointerPump:
                pointerType = SrLibAmigaSpriteAssetMousePointerRag;
                break;
            case SrLibAmigaSpriteAssetMousePointerRag:
                pointerType = SrLibAmigaSpriteAssetMousePointerThumbsUp;
                break;
            case SrLibAmigaSpriteAssetMousePointerThumbsUp:
                pointerType = SrLibAmigaSpriteAssetMousePointerThumbsDown;
                break;
            case SrLibAmigaSpriteAssetMousePointerThumbsDown:
                pointerType = SrLibAmigaSpriteAssetMousePointerHand;
                break;
            default:
                break;
        }
        cursorTexture = pointerTextures[pointerType];
    }
    
    // Handle gamepad events
    if (event->type == SDL_JOYAXISMOTION) {
        // Activate gamepad mode when joystick is used
        gamepadActive = true;
    }
    
    if (event->type == SDL_JOYBUTTONDOWN) {
        gamepadActive = true;
    }
}

void Mouse::UpdateGamepad(Scene* scene) {

    Uint32 currentTime = SDL_GetTicks();

    // Open the joystick once and keep it open across frames; only reopen if
    // it was disconnected (avoids opening/closing the device every frame).
    if (joystickHandle && !SDL_JoystickGetAttached(joystickHandle)) {
        SDL_JoystickClose(joystickHandle);
        joystickHandle = nullptr;
    }
    if (!joystickHandle) {
        joystickHandle = SDL_JoystickOpen(0);
        if (joystickHandle) {
            LOG_INFO("Joystick opened successfully: %s", SDL_JoystickName(joystickHandle));
        }
    }

    SDL_Joystick* joystick = joystickHandle;
    if (!joystick) {
        gamepadDebug.connected = false;
        return;
    }

    gamepadDebug.connected = true;
    
    DPadDirection currentDirection = DPadDirection::NONE;
    
    // Check D-pad hat
    Uint8 hatState = SDL_JoystickGetHat(joystick, 0);
    gamepadDebug.hatState = hatState;
    
    if (hatState & SDL_HAT_UP) {
        currentDirection = DPadDirection::UP;
        gamepadDebug.lastAction = "D-pad UP";
    } else if (hatState & SDL_HAT_DOWN) {
        currentDirection = DPadDirection::DOWN;
        gamepadDebug.lastAction = "D-pad DOWN";
    } else if (hatState & SDL_HAT_LEFT) {
        currentDirection = DPadDirection::LEFT;
        gamepadDebug.lastAction = "D-pad LEFT";
    } else if (hatState & SDL_HAT_RIGHT) {
        currentDirection = DPadDirection::RIGHT;
        gamepadDebug.lastAction = "D-pad RIGHT";
    }
    
    // Handle D-pad input for zone jumping
    if (currentDirection != DPadDirection::NONE) {
        if (currentDirection != lastDPadDirection || 
            (currentTime - lastDPadTime) > DPAD_REPEAT_DELAY) {
            
            JumpToClosestZoneInDirection(scene, currentDirection);
            lastDPadTime = currentTime;
        }
    }
    
    lastDPadDirection = currentDirection;
    
    // Handle left analog stick for mouse movement
    Sint16 leftX = SDL_JoystickGetAxis(joystick, 0); // Left stick X
    Sint16 leftY = SDL_JoystickGetAxis(joystick, 1); // Left stick Y
    
    gamepadDebug.leftX = leftX;
    gamepadDebug.leftY = leftY;
    
    const Sint16 DEADZONE = 8192; // 25% of max value for more responsive movement
    const float MOVEMENT_SPEED = 2.0f; // Pixels per frame
    
    // Apply deadzone
    if (abs(leftX) > DEADZONE) {
        float normalizedX = (float)(leftX - (leftX > 0 ? DEADZONE : -DEADZONE)) / (32767 - DEADZONE);
        rect.x += (int)(normalizedX * MOVEMENT_SPEED);
        gamepadDebug.lastAction = "Analog X: " + std::to_string((int)(normalizedX * 100));
    }
    
    if (abs(leftY) > DEADZONE) {
        float normalizedY = (float)(leftY - (leftY > 0 ? DEADZONE : -DEADZONE)) / (32767 - DEADZONE);
        rect.y += (int)(normalizedY * MOVEMENT_SPEED);
        gamepadDebug.lastAction = "Analog Y: " + std::to_string((int)(normalizedY * 100));
    }
    
    // Clamp mouse position to screen bounds
    rect.x = std::max(0, std::min(rect.x, 320 - rect.w));
    rect.y = std::max(0, std::min(rect.y, 200 - rect.h));
    
    // Handle gamepad button presses
    gamepadDebug.buttonA = SDL_JoystickGetButton(joystick, 0);
    gamepadDebug.buttonB = SDL_JoystickGetButton(joystick, 1);
    gamepadDebug.buttonX = SDL_JoystickGetButton(joystick, 2);
    gamepadDebug.buttonY = SDL_JoystickGetButton(joystick, 3);
    gamepadDebug.buttonLB = SDL_JoystickGetButton(joystick, 4);
    gamepadDebug.buttonRB = SDL_JoystickGetButton(joystick, 5);
    
    if (gamepadDebug.buttonA) { // A button (usually)
        gamepadDebug.lastAction = "Button A (Click)";
        // Simulate left click at current position
        SDL_Event clickEvent;
        clickEvent.type = SDL_MOUSEBUTTONDOWN;
        clickEvent.button.button = SDL_BUTTON_LEFT;
        clickEvent.button.x = rect.x + rect.w / 2;
        clickEvent.button.y = rect.y + rect.h / 2;
        clickEvent.button.clicks = 1;
        clickEvent.button.state = SDL_PRESSED;
        scene->PushEvent(&clickEvent);
        
        // Also send button up event
        clickEvent.type = SDL_MOUSEBUTTONUP;
        clickEvent.button.state = SDL_RELEASED;
        scene->PushEvent(&clickEvent);
    }
    
    if (gamepadDebug.buttonB) { // B button (usually)
        gamepadDebug.lastAction = "Button B (Right Click)";
        
        // Check if there's an active dialog - if so, try to dismiss it
        Dialog* activeDialog = scene->GetActiveDialog();
        if (activeDialog) {
            // Try to dismiss dialog with B button
            SDL_Event keyEvent;
            keyEvent.type = SDL_KEYDOWN;
            keyEvent.key.keysym.sym = SDLK_ESCAPE;
            scene->PushEvent(&keyEvent);
        } else {
            // Simulate right click for pointer change
            SDL_Event clickEvent;
            clickEvent.type = SDL_MOUSEBUTTONDOWN;
            clickEvent.button.button = SDL_BUTTON_RIGHT;
            clickEvent.button.x = rect.x + rect.w / 2;
            clickEvent.button.y = rect.y + rect.h / 2;
            clickEvent.button.clicks = 1;
            clickEvent.button.state = SDL_PRESSED;
            scene->PushEvent(&clickEvent);
            
            // Also send button up event
            clickEvent.type = SDL_MOUSEBUTTONUP;
            clickEvent.button.state = SDL_RELEASED;
            scene->PushEvent(&clickEvent);
        }
    }
    
    // Additional button mappings
    if (gamepadDebug.buttonX) { // X button (usually)
        gamepadDebug.lastAction = "Button X (Jump to Zone)";
        // Jump to closest zone
        JumpToClosestZone(scene);
    }
    
    if (gamepadDebug.buttonY) { // Y button (usually)
        gamepadDebug.lastAction = "Button Y (Toggle Gamepad)";
        // Toggle gamepad mode
        gamepadActive = !gamepadActive;
    }
    
    // Shoulder buttons for scene navigation
    if (gamepadDebug.buttonLB) { // Left shoulder (usually)
        gamepadDebug.lastAction = "Button LB (Garage)";
        // Go to garage
        SDL_Event keyEvent;
        keyEvent.type = SDL_KEYUP;
        keyEvent.key.keysym.sym = SDLK_g;
        scene->PushEvent(&keyEvent);
    }
    
    if (gamepadDebug.buttonRB) { // Right shoulder (usually)
        gamepadDebug.lastAction = "Button RB (Diner)";
        // Go to diner
        SDL_Event keyEvent;
        keyEvent.type = SDL_KEYUP;
        keyEvent.key.keysym.sym = SDLK_d;
        scene->PushEvent(&keyEvent);
    }
    
    // Add Enter key simulation for dialog confirmation
    if (SDL_JoystickGetButton(joystick, 6)) { // Start button (usually)
        gamepadDebug.lastAction = "Start Button (Enter)";
        SDL_Event keyEvent;
        keyEvent.type = SDL_KEYDOWN;
        keyEvent.key.keysym.sym = SDLK_RETURN;
        scene->PushEvent(&keyEvent);
    }
}

std::vector<DebugRow> Mouse::GetGamepadDebugRows() const {
    std::vector<DebugRow> rows;
    rows.push_back({"connected", gamepadDebug.connected ? "yes" : "no"});
    rows.push_back({"active", gamepadActive ? "yes" : "no"});
    rows.push_back({"mouse", std::to_string(rect.x) + ", " + std::to_string(rect.y)});

    if (gamepadDebug.connected) {
        rows.push_back({"left stick", "X=" + std::to_string(gamepadDebug.leftX) +
                                      " Y=" + std::to_string(gamepadDebug.leftY)});
        rows.push_back({"hat", std::to_string(gamepadDebug.hatState)});

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

float Mouse::CalculateDistance(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

const Zone* Mouse::FindClosestZone(const std::vector<Zone>& zones, int mouseX, int mouseY) {
    const Zone* closest = nullptr;
    float closestDistance = std::numeric_limits<float>::max();

    for (const auto& zone : zones) {
        if (!zone.IsInsideZone(mouseX, mouseY)) continue;

        SDL_Rect zoneRect = zone.GetRect();
        int zoneCenterX = zoneRect.x + zoneRect.w / 2;
        int zoneCenterY = zoneRect.y + zoneRect.h / 2;

        float distance = CalculateDistance(mouseX, mouseY, zoneCenterX, zoneCenterY);
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = &zone;
        }
    }

    return closest;
}

void Mouse::JumpToClosestZoneInDirection(Scene* scene, DPadDirection direction) {
    if (!scene) return;
    
    const std::vector<Zone>& zones = scene->GetZones();
    if (zones.empty()) return;
    
    int currentX = rect.x + rect.w / 2;
    int currentY = rect.y + rect.h / 2;
    
    const Zone* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();

    for (const auto& zone : zones) {
        if (!zone.IsInsideZone(currentX, currentY)) continue;

        SDL_Rect zoneRect = zone.GetRect();
        int zoneCenterX = zoneRect.x + zoneRect.w / 2;
        int zoneCenterY = zoneRect.y + zoneRect.h / 2;

        int dx = zoneCenterX - currentX;
        int dy = zoneCenterY - currentY;

        bool inDirection = false;
        switch (direction) {
            case DPadDirection::UP:
                inDirection = (dy < 0);
                break;
            case DPadDirection::DOWN:
                inDirection = (dy > 0); // Zone is below current position
                break;
            case DPadDirection::LEFT:
                inDirection = (dx < 0); // Zone is to the left of current position
                break;
            case DPadDirection::RIGHT:
                inDirection = (dx > 0); // Zone is to the right of current position
                break;
            default:
                continue;
        }
        
        if (!inDirection) continue;
        
        // Calculate distance
        float distance = CalculateDistance(currentX, currentY, zoneCenterX, zoneCenterY);
        
        // Prefer zones that are more aligned with the direction
        float alignmentScore = 0;
        switch (direction) {
            case DPadDirection::UP:
            case DPadDirection::DOWN:
                alignmentScore = abs(dx); // Prefer zones with less horizontal offset
                break;
            case DPadDirection::LEFT:
            case DPadDirection::RIGHT:
                alignmentScore = abs(dy); // Prefer zones with less vertical offset
                break;
            case DPadDirection::NONE:
                break;
        }
        
        // Combined score: distance + alignment penalty
        float score = distance + alignmentScore * 0.5f;
        
        if (score < bestScore) {
            bestScore = score;
            best = &zone;
        }
    }

    if (!best) {
        JumpToClosestZone(scene);
        return;
    }

    {
        SDL_Rect zoneRect = best->GetRect();
        int zoneCenterX = zoneRect.x + zoneRect.w / 2;
        int zoneCenterY = zoneRect.y + zoneRect.h / 2;
        rect.x = zoneCenterX - rect.w / 2;
        rect.y = zoneCenterY - rect.h / 2;
        rect.x = std::max(0, std::min(rect.x, 320 - rect.w));
        rect.y = std::max(0, std::min(rect.y, 200 - rect.h));
    }
}

void Mouse::JumpToClosestZone(Scene* scene) {
    if (!scene) return;

    const std::vector<Zone>& zones = scene->GetZones();
    if (zones.empty()) {
        rect.x = (GAME_WIDTH - rect.w) / 2;
        rect.y = (GAME_HEIGHT - rect.h) / 2;
        return;
    }

    int currentX = rect.x + rect.w / 2;
    int currentY = rect.y + rect.h / 2;

    const Zone* closest = FindClosestZone(zones, currentX, currentY);

    if (!closest) {
        rect.x = (GAME_WIDTH - rect.w) / 2;
        rect.y = (GAME_HEIGHT - rect.h) / 2;
        return;
    }

    SDL_Rect zoneRect = closest->GetRect();
    int zoneCenterX = zoneRect.x + zoneRect.w / 2;
    int zoneCenterY = zoneRect.y + zoneRect.h / 2;
    rect.x = zoneCenterX - rect.w / 2;
    rect.y = zoneCenterY - rect.h / 2;
    rect.x = std::max(0, std::min(rect.x, 320 - rect.w));
    rect.y = std::max(0, std::min(rect.y, 200 - rect.h));
}