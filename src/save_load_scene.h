#pragma once

#include "scene.h"
#include "dialogs/list_dialog.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// The save/load screen, on the original's California Highway Patrol roster
// background (lib.p 300, unused by the original's own save flow). Replaces the
// load-game ListDialog that used to live inside Garage.
//
// Loads two formats: RodLegacy's own .toml saves (save_file.h) and real Amiga
// .sav files, either loose in the game directory or shipped in disk2's ADF
// image. Only the .toml format is written - see save_loader.h for why the
// original format is read-only here.
class SaveLoadScene : public Scene {
public:
    enum class Mode { Load, Save };

    SaveLoadScene();

    void Init(bool debugMode) override;
    // Call before state.MoveToScene(SCENE_SAVE_LOAD). returnTo is where
    // "Forget it" (and a finished load/save) goes back to.
    void Configure(Mode mode, SrLibGameScene returnTo);

    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void OnFadeComplete() override;

private:
    // One row of the list: a RodLegacy .toml save, a loose .sav in the game
    // directory, a sample .sav inside disk2's ADF, or (save mode only) the
    // "new save" row.
    struct Entry {
        enum class Kind { Modern, LegacyFile, LegacyDisk2, NewSave } kind = Kind::Modern;
        std::string label;
        std::filesystem::path path;  // Modern / LegacyFile
        std::string adfFileName;     // LegacyDisk2
    };

    Mode mode = Mode::Load;
    SrLibGameScene returnTo = SCENE_GARAGE;

    std::vector<Entry> entries;
    // Scanned once from Init(): listing an ADF means mounting it through a
    // temp file, and doing that on first open turned a startup hitch (already
    // hidden in loading) into a visible delay when opening the screen. The
    // image never changes mid-session.
    std::vector<Entry> disk2Samples;
    std::unique_ptr<ListDialog> listDialog;
    // Save mode: "Overwrite <name> ?" box, put up when the name being saved
    // under is already taken by one of our own saves.
    std::unique_ptr<Dialog> confirmDialog;

    // Save mode: the name typed into the "new save" row. Starts as the
    // player's own name so hitting Save straight away does the obvious
    // thing, and is what the finished save is named after.
    std::string newSaveName;
    static constexpr size_t kMaxSaveNameLength = 20;
    static constexpr Uint64 kCursorBlinkMs = 400;

    void ScanDisk2Samples();
    void RebuildList();
    void BuildDialog();
    // Re-labels the new-save row in place (with its caret) without tearing
    // the dialog down, so typing doesn't lose the list's scroll or selection.
    void RefreshNewSaveRow();
    // Blinking block cursor after the typed name on the new-save row.
    void DrawNameCursor();
    // Index of our own save carrying `name` (trimmed, case-insensitive), or -1.
    int FindSaveNamed(const std::string& name) const;
    void Commit();       // load or save the selected row
    // Saves under `name`, which becomes the player name. A save already using
    // that name is overwritten (after confirming) rather than joined by a
    // second row reading the same - .sav files are read-only and can't clash.
    void SaveUnderName(const std::string& name);
    void ConfirmOverwrite(const std::filesystem::path& target, const std::string& name);
    void CancelConfirm();
    void WriteSave(const std::filesystem::path& target, const std::string& name);
    void Leave();        // "Forget it"
    void GoTo(SrLibGameScene scene);
    bool LoadEntry(const Entry& e);
};
