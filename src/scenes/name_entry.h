/*
 * RodLegacy - The new-game name entry screen (DMV), with the car set choice.
 *
 * Created: 2026-08-05
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "ui/label.h"
#include "ui/button.h"
#include "ui/checkbox.h"
#include <memory>
#include <string>
#include <vector>

class NameEntry : public Scene {
private:
    static constexpr size_t MAX_NAME_LENGTH = 16;

    std::string typedName;
    Label nameLabel;
    std::unique_ptr<Button> oldGameButton;
    std::unique_ptr<Button> okButton;
    // One row per optional car set whose data is actually installed.
    struct SetRow {
        int carSet = -1;
        bool enabled = false;
        Label label;
        std::unique_ptr<Checkbox> checkbox;
    };
    std::vector<std::unique_ptr<SetRow>> setRows;

    void RefreshNameLabel();
    void HandleOk();

public:
    NameEntry();
    ~NameEntry() = default;

    void Init(bool debugMode) override;
    void OnSceneEnter() override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnSceneExit() override;
};
