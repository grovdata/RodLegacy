#pragma once

#include "scene.h"
#include "ui/label.h"
#include "ui/button.h"
#include <memory>
#include <string>

// DMV, the new-game entry screen
class NameEntry : public Scene {
private:
    static constexpr size_t MAX_NAME_LENGTH = 16;

    std::string typedName;
    Label nameLabel;
    std::unique_ptr<Button> oldGameButton;
    std::unique_ptr<Button> okButton;

    void RefreshNameLabel();
    void HandleOk();

public:
    NameEntry();
    ~NameEntry() = default;

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnSceneExit() override;
};
