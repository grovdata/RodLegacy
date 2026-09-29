/*
 * RodLegacy - The save and load screen for TOML saves and original Amiga .sav files.
 *
 * Created: 2026-09-01
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "dialogs/list_dialog.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Loads RodLegacy .toml saves and Amiga .sav files (loose or from disk2's ADF). Only .toml is written.
class SaveLoadScene : public Scene {
public:
    enum class Mode { Load, Save };

    SaveLoadScene();

    void Init(bool debugMode) override;
    // Call before MoveToScene(SCENE_SAVE_LOAD). returnTo is where "Forget it" and a finished load/save go.
    void Configure(Mode mode, SrLibGameScene returnTo);

    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnFadeComplete() override;

private:
    struct Entry {
        enum class Kind { Modern, LegacyFile, LegacyDisk2, NewSave } kind = Kind::Modern;
        std::string label;
        std::filesystem::path path;  // Modern / LegacyFile
        std::string adfFileName;     // LegacyDisk2
    };

    Mode mode = Mode::Load;
    SrLibGameScene returnTo = SCENE_GARAGE;

    std::vector<Entry> entries;
    // Scanned once in Init(): listing the ADF goes through a temp file and is slow.
    std::vector<Entry> disk2Samples;
    std::unique_ptr<ListDialog> listDialog;
    std::unique_ptr<Dialog> confirmDialog;

    // Starts as the player's name, and names the finished save.
    std::string newSaveName;
    static constexpr size_t kMaxSaveNameLength = 20;
    static constexpr Uint64 kCursorBlinkMs = 400;

    void ScanDisk2Samples();
    void RebuildList();
    void BuildDialog();
    // In place, so typing keeps the list's scroll and selection.
    void RefreshNewSaveRow();
    void DrawNameCursor();
    // Index of our own save carrying `name` (trimmed, case-insensitive), or -1.
    int FindSaveNamed(const std::string& name) const;
    void Commit();       // load or save the selected row
    // An existing save with that name is overwritten after confirming.
    void SaveUnderName(const std::string& name);
    void ConfirmOverwrite(const std::filesystem::path& target, const std::string& name);
    void CancelConfirm();
    void WriteSave(const std::filesystem::path& target, const std::string& name);
    void Leave();        // "Forget it"
    void GoTo(SrLibGameScene scene);
    bool LoadEntry(const Entry& e);
};
