#pragma once

#include "../global.h"
#include <string>
#include <functional>

class Zone {
    std::string id;
    std::string text;
    SDL_Rect rect;
    bool enabled;
public:
    std::function<void()> onClick;

    Zone(std::string id, std::string text, SDL_Rect rect, bool enabled = true)
        : id(std::move(id)), text(std::move(text)), rect(rect), enabled(enabled) {}

    bool IsInsideZone(int x, int y) const {
        if (!enabled) return false;
        return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
    }

    const std::string& GetId() const { return id; }
    const std::string& GetText() const { return text; }
    void SetText(const std::string& t) { text = t; }
    SDL_Rect GetRect() const { return rect; }
    void SetRect(SDL_Rect r) { rect = r; }
    void SetEnabled(bool e) { enabled = e; }
    bool IsEnabled() const { return enabled; }
};
