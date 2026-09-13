#pragma once

#include "scenes/scene.h"
#include "ui/label.h"
#include "core/state.h"
#include <vector>

class KingOfCounty : public Scene {
private:
    bool wonGame;
    int newEntryIndex = -1;  // index in table of newly inserted entry, -1 if not inserted

    // One label per row: "NAME                 SCORE"
    // We render name and score separately for alignment.
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
