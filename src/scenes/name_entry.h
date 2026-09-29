#pragma once

#include "scenes/scene.h"
#include "ui/label.h"
#include "ui/button.h"
#include "ui/checkbox.h"
#include <memory>
#include <string>

class NameEntry : public Scene {
private:
    static constexpr size_t MAX_NAME_LENGTH = 16;

    std::string typedName;
    Label nameLabel;
    std::unique_ptr<Button> oldGameButton;
    std::unique_ptr<Button> okButton;
    // Only with SE data installed. Chosen here because it decides the car spec table the game is generated against.
    std::unique_ptr<Checkbox> seCarsCheckbox;
    bool seCarsEnabled = false;
    Label seCarsLabel;

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
