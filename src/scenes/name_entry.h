#pragma once

#include "scenes/scene.h"
#include "ui/label.h"
#include "ui/button.h"
#include "ui/checkbox.h"
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
    // Only built when the SE data is installed. The set it switches decides the
    // car spec table this game is generated against, which is why it is chosen
    // here rather than in Options - see State::RebuildCarSets.
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
