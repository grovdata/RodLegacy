/*
 * RodLegacy - The hall of fame scene: KingOfCounty shows and updates the King Street Rodders table.
 *
 * Created: 2026-04-02
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "ui/label.h"
#include "core/state.h"
#include <vector>

class KingOfCounty : public Scene {
private:
    bool wonGame;
    int newEntryIndex = -1;

    struct Row {
        Label name;
        Label score;
        bool highlight;
    };
    std::vector<Row> rows;

public:
    explicit KingOfCounty(bool wonGame);
    ~KingOfCounty() = default;

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
};
