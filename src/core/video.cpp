#include "core/video.h"
#include "core/assets.h"
#include "core/global.h"

#include <algorithm>
#include <cstdlib>

namespace {
int scale = video::kDefaultScale;
// ponytail: guessed title bar height while the window (and so its real border) does not exist yet.
constexpr int kTitleBarAllowance = 32;
}

int video::MaxScale() {
    const int display = win ? std::max(0, SDL_GetWindowDisplayIndex(win)) : 0;
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable) != 0) return 1;
    int top = 0;
    if (!win || isFullScreen || SDL_GetWindowBordersSize(win, &top, nullptr, nullptr, nullptr) != 0 || top <= 0) {
        top = kTitleBarAllowance;
    }
    return MaxScaleFor(usable.w, usable.h, top);
}

int video::Scale() {
    return scale;
}

void video::SetScale(int s) {
    scale = std::clamp(s, 1, MaxScale());
    if (!win) return;
    SDL_SetWindowSize(win, GAME_WIDTH * scale, GAME_HEIGHT * scale);
    if (!isFullScreen) {
        const int display = std::max(0, SDL_GetWindowDisplayIndex(win));
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    }
}

bool video::Fullscreen() {
    return isFullScreen;
}

void video::SetFullscreen(bool on) {
    isFullScreen = on;
    if (win) SDL_SetWindowFullscreen(win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

int video::SavedScale() {
    return std::atoi(AssetManager::ReadSetting("window_scale").c_str());
}

bool video::SavedFullscreen() {
    return AssetManager::ReadSetting("fullscreen") == "1";
}

void video::Save() {
    AssetManager::WriteSetting("window_scale", std::to_string(scale));
    AssetManager::WriteSetting("fullscreen", isFullScreen ? "1" : "0");
}
