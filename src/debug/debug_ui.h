/*
 * RodLegacy - ImGui debug UI: panels, rows, toggles and indicators around the game view, or an empty stub.
 *
 * Created: 2026-07-30
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

// ImGui debug UI: the game renders into an offscreen texture shown inside a dockspace.
// Only with -Ddebug_ui=true; otherwise the stub below makes every call an empty inline.

#include <SDL2/SDL.h>

#include <functional>
#include <string>
#include <vector>

enum class DebugDock {
    Left,   // player and global information
    Right,  // information about the active scene
};

// A row with an empty value is a section label.
struct DebugRow {
    std::string key;
    std::string value;
};

// Toggles are data, not callbacks, so producers compile without the debug UI.
struct DebugToggle {
    std::string label;
    bool* value = nullptr;  // not owned
};

struct DebugIndicator {
    std::string label;
    bool active = false;
};

struct DebugButton {
    std::string label;
    std::function<void()> onClick;
};

// `rows` and `toggles` come last so brace initialisations of title/lines/dock still compile.
struct DebugPanel {
    std::string title;
    std::vector<std::string> lines;
    DebugDock dock = DebugDock::Right;
    std::vector<DebugRow> rows = {};
    std::vector<DebugToggle> toggles = {};
    std::vector<DebugButton> buttons = {};
    // Not owned; must outlive the frame.
    SDL_Texture* image = nullptr;
    int imageScale = 2;
    std::vector<DebugIndicator> indicators = {};
};

// Code polling SDL_GetKeyboardState never sees events, so it must check this first.
bool DebugUiWantsKeyboard();

#ifdef RODLEGACY_DEBUG_UI

class DebugUi {
public:
    ~DebugUi();

    // On failure Active() stays false and the game runs without a debug UI.
    bool Init(SDL_Window* window, SDL_Renderer* renderer, int gameWidth, int gameHeight);
    void Shutdown();

    bool Active() const { return active; }

    // True when the event was for the UI.
    bool ProcessEvent(const SDL_Event& event);

    SDL_Texture* GameTexture() const { return gameTexture; }

    // Call after the game is drawn into GameTexture() and the target is reset to the backbuffer.
    void Draw(const std::vector<DebugPanel>& panels);

    // Empty until Draw() has run.
    SDL_Rect GameViewportRect() const { return gameViewport; }
    bool GameViewportHovered() const { return gameHovered; }

    bool WantsKeyboard() const;
    bool WantsMouse() const;

private:
    bool active = false;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* gameTexture = nullptr;
    int gameW = 0;
    int gameH = 0;
    SDL_Rect gameViewport {0, 0, 0, 0};
    bool gameHovered = false;

};

#else  // !RODLEGACY_DEBUG_UI

class DebugUi {
public:
    bool Init(SDL_Window*, SDL_Renderer*, int, int) { return false; }
    void Shutdown() {}
    bool Active() const { return false; }
    bool ProcessEvent(const SDL_Event&) { return false; }
    SDL_Texture* GameTexture() const { return nullptr; }
    void Draw(const std::vector<DebugPanel>&) {}
    SDL_Rect GameViewportRect() const { return SDL_Rect{0, 0, 0, 0}; }
    bool GameViewportHovered() const { return false; }
    bool WantsKeyboard() const { return false; }
    bool WantsMouse() const { return false; }
};

inline bool DebugUiWantsKeyboard() { return false; }

#endif  // RODLEGACY_DEBUG_UI
