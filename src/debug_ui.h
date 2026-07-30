#pragma once

// Dear ImGui debug UI, hosted in the game window: the game is rendered into an
// offscreen texture and shown as an image inside a dockspace, with debug panels
// docked around it.
//
// Compiled only when the build is configured with -Ddebug_ui=true. Without that
// option the stub at the bottom of this header takes over, every method is an
// empty inline, and the binary links no ImGui - so the call sites in main.cpp
// need no conditionals.

#include <SDL2/SDL.h>

#include <functional>
#include <string>
#include <vector>

// A named block of text lines for one docked panel.
struct DebugPanel {
    std::string title;
    std::vector<std::string> lines;
};

// True while a debug UI text field has keyboard focus. Code that polls
// SDL_GetKeyboardState directly never sees events, so it must consult this
// before acting on key state. Always false in builds without the debug UI.
bool DebugUiWantsKeyboard();

#ifdef RODLEGACY_DEBUG_UI

class DebugUi {
public:
    ~DebugUi();

    // Creates the ImGui context, the SDL2/SDL_Renderer backends and the
    // offscreen game texture. Returns false if anything failed, in which case
    // Active() stays false and the caller carries on without a debug UI.
    bool Init(SDL_Window* window, SDL_Renderer* renderer, int gameWidth, int gameHeight);
    void Shutdown();

    bool Active() const { return active; }

    // Feed every SDL event here first. Returns true when the event was for the
    // UI and the game should ignore it.
    bool ProcessEvent(const SDL_Event& event);

    // Render target for the game. Draw the scene into this instead of the
    // backbuffer; it is what gets shown inside the dockspace.
    SDL_Texture* GameTexture() const { return gameTexture; }

    // Lays out the dockspace, draws the game image and the supplied panels, and
    // submits the draw data. Call once per frame, after the game has been drawn
    // into GameTexture() and the render target has been reset to the backbuffer.
    void Draw(const std::vector<DebugPanel>& panels,
              const std::function<void()>& drawControls = {});

    // Where the game image landed on screen this frame, for mapping window
    // mouse coordinates back into game space. Empty until Draw() has run.
    SDL_Rect GameViewportRect() const { return gameViewport; }
    bool GameViewportHovered() const { return gameHovered; }

    // True while ImGui owns the keyboard (a text field has focus). The game
    // polls SDL_GetKeyboardState directly, which bypasses events entirely, so it
    // has to consult this before acting on key state.
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
    bool dockLayoutDone = false;
};

#else  // !RODLEGACY_DEBUG_UI

// Stub for builds without the debug UI. Active() is constant false, so the
// guarded blocks in main.cpp fold away and no ImGui symbol is referenced.
class DebugUi {
public:
    bool Init(SDL_Window*, SDL_Renderer*, int, int) { return false; }
    void Shutdown() {}
    bool Active() const { return false; }
    bool ProcessEvent(const SDL_Event&) { return false; }
    SDL_Texture* GameTexture() const { return nullptr; }
    void Draw(const std::vector<DebugPanel>&, const std::function<void()>& = {}) {}
    SDL_Rect GameViewportRect() const { return SDL_Rect{0, 0, 0, 0}; }
    bool GameViewportHovered() const { return false; }
    bool WantsKeyboard() const { return false; }
    bool WantsMouse() const { return false; }
};

inline bool DebugUiWantsKeyboard() { return false; }

#endif  // RODLEGACY_DEBUG_UI
