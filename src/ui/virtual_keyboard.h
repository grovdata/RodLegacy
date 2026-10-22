/*
 * RodLegacy - On-screen keyboard for typing with a gamepad: VirtualKeyboard.
 *
 * Created: 2026-10-22
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include <SDL.h>
#include <string>
#include <vector>

class Scene;

class VirtualKeyboard {
public:
    void SetNumeric(bool numeric);

    void Move(int dx, int dy);
    const std::string& Selected() const;
    std::string Typed(const std::string& key) const;

    // Takes the pad while text input is on.
    void Update(Scene* scene);
    void Draw();

    // Shown while text input is on, unless the player put it away to reach the buttons.
    bool Visible();
    bool AtTop() const { return row == 0; }
    void ToggleHidden() { hidden = !hidden; }

    bool shift = true;

private:
    std::vector<std::vector<std::string>> rows;
    bool numeric = false;
    bool hidden = false;
    int row = 0;
    int col = 0;
    Uint32 lastMoveTime = 0;
    int lastDx = 0;
    int lastDy = 0;

    void Press(Scene* scene, const std::string& key);
    void Reset();
};

extern VirtualKeyboard virtualKeyboard;
