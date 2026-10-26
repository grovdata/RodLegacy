#include "scenes/save_load_scene.h"

#include "core/state.h"
#include "core/assets.h"
#include "core/game_clock.h"
#include "core/save_file.h"
#include "core/save_loader.h"
#include "parsers/adf.h"
#include "ui/hud.h"
#include "ui/main_font.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {

constexpr SDL_Rect kListRect      = {79, 37, 160, 142};
constexpr SDL_Rect kActionRect    = {240, 100, 56, 39};  // Load or Save
constexpr SDL_Rect kForgetItRect  = {240, 140, 56, 39};

constexpr SDL_Color kTextColor         = {0x00, 0x00, 0x00, 0xFF};
constexpr SDL_Color kSelectionBoxColor = {0x00, 0xAF, 0xEF, 0xFF};

// The .sav header space-pads names.
std::string TrimTrailingSpaces(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::string LegacyLabel(const std::string& filename, const std::string& rawSaveName) {
    const std::string trimmed = TrimTrailingSpaces(rawSaveName);
    if (trimmed.empty()) return filename;
    return trimmed;
}

std::string TrimSpaces(std::string s) {
    s = TrimTrailingSpaces(std::move(s));
    s.erase(0, s.find_first_not_of(' '));
    return s;
}

// Compared trimmed and case-insensitively.
std::string NameKey(std::string s) {
    s = TrimSpaces(std::move(s));
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Keeps a player name usable as a filename on every platform we build for.
std::string SanitiseForFilename(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(c);
        else if (c == '-' || c == '_') out.push_back(c);
        else if (c == ' ') out.push_back('_');
    }
    if (out.empty()) out = "save";
    return out;
}

}

SaveLoadScene::SaveLoadScene()
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP,
                    ROSTER_SCREEN_CALIFORNIA_HIGHWAY_PATROL}) {}

void SaveLoadScene::Init(bool debugMode) {
    Scene::Init(debugMode);
    ScanDisk2Samples();
}

void SaveLoadScene::Configure(Mode newMode, SrLibGameScene newReturnTo) {
    const Uint64 t0 = SDL_GetTicks64();
    mode = newMode;
    returnTo = newReturnTo;
    // Hidden rather than destroyed: Configure can run from the confirm dialog's own callback.
    if (confirmDialog) confirmDialog->Hide();
    newSaveName = state.GetSaveName();
    if (newSaveName.size() > kMaxSaveNameLength) newSaveName.resize(kMaxSaveNameLength);
    if (mode == Mode::Save) SDL_StartTextInput();
    else                    SDL_StopTextInput();
    RebuildList();
    const Uint64 t1 = SDL_GetTicks64();
    BuildDialog();
    const Uint64 t2 = SDL_GetTicks64();
    // Open on the new-save row, or on the save the name already belongs to.
    if (mode == Mode::Save && listDialog && !entries.empty()) {
        const int clash = FindSaveNamed(newSaveName);
        listDialog->SetSelectedIndex(clash >= 0 ? clash
                                                : static_cast<int>(entries.size()) - 1);
    }
    SetActiveDialog(listDialog.get());
    LOG_INFO("SaveLoadScene::Configure: RebuildList %llums, BuildDialog %llums, total %llums",
             (unsigned long long)(t1 - t0), (unsigned long long)(t2 - t1),
             (unsigned long long)(SDL_GetTicks64() - t0));
}

void SaveLoadScene::ScanDisk2Samples() {
    const Uint64 scanStart = SDL_GetTicks64();
    disk2Samples.clear();
    const auto& disk2 = state.GetAssets().GetDisk2Adf();
    if (disk2.empty()) return;

    AdfFileNameList* names = list_adf_files_from_buffer(disk2.data(), disk2.size(), ".sav");
    if (!names) {
        LOG_INFO("SaveLoadScene: failed to list .sav files on disk2");
        return;
    }
    for (size_t i = 0; i < names->count; ++i) {
        Entry e;
        e.kind = Entry::Kind::LegacyDisk2;
        e.adfFileName = names->names[i];

        std::string saveName;
        AdfFileData* data = extract_adf_file_from_buffer(disk2.data(), disk2.size(), names->names[i]);
        if (data) {
            try {
                std::vector<uint8_t> bytes(data->data, data->data + data->size);
                saveName = SaveGameLoader::Parse(bytes).name;
            } catch (const SaveLoadError& ex) {
                LOG_INFO("SaveLoadScene: couldn't read name from %s: %s", names->names[i], ex.what());
            }
            free_adf_file_data(data);
        }
        e.label = "[adf] " + LegacyLabel(names->names[i], saveName);
        disk2Samples.push_back(e);
    }
    free_adf_file_name_list(names);
    LOG_INFO("SaveLoadScene: %d sample save(s) on disk2 in %llums", (int)disk2Samples.size(),
             (unsigned long long)(SDL_GetTicks64() - scanStart));
}

void SaveLoadScene::RebuildList() {
    entries.clear();

    const SrGame game = state.GetAssets().GetGame();
    const fs::path savesDir = AssetManager::GetSavesDir();
    // Saves from before the per-game folders sit in savesDir itself.
    std::vector<fs::path> paths = ListSaveFiles(savesDir / SrGameSaveKey(game));
    for (const auto& path : ListSaveFiles(savesDir)) paths.push_back(path);
    for (const auto& path : paths) {
        Entry e;
        e.kind = Entry::Kind::Modern;
        e.path = path;
        e.label = path.stem().string();
        try {
            const SaveSummary s = PeekSave(path);
            if (!SrGameSaveKeyMatches(s.game, game)) continue;
            e.label = s.saveName;
        } catch (const SaveLoadError& ex) {
            LOG_INFO("SaveLoadScene: couldn't read %s: %s", path.string().c_str(), ex.what());
            e.label += "  (unreadable)";
            continue;  // Nothing useful can be done with it in either mode.
        }
        entries.push_back(e);
    }

    if (mode == Mode::Save) {
        Entry e;
        e.kind = Entry::Kind::NewSave;
        e.label = newSaveName;  // the block cursor is drawn on top, see DrawScene
        entries.push_back(e);
        return;
    }

    // Original .sav files, load-only: loose ones first, then the disk2 samples.
    std::error_code ec;
    const fs::path base = state.GetAssets().GetBasePath();
    for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->path().extension() != ".sav") continue;
        Entry e;
        e.kind = Entry::Kind::LegacyFile;
        e.path = it->path();

        std::string saveName;
        try {
            saveName = SaveGameLoader::Load(it->path()).name;
        } catch (const SaveLoadError& ex) {
            LOG_INFO("SaveLoadScene: couldn't read name from %s: %s",
                     it->path().string().c_str(), ex.what());
        }
        e.label = LegacyLabel(it->path().filename().string(), saveName);
        entries.push_back(e);
    }

    for (const Entry& e : disk2Samples) entries.push_back(e);
}

void SaveLoadScene::BuildDialog() {
    AssetManager& assets = state.GetAssets();
    const bool saving = (mode == Mode::Save);

    Bitmap* actionSprite = assets.GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP,
         saving ? ROSTER_SCREEN_BUTTON_SAVE : ROSTER_SCREEN_BUTTON_LOAD});
    Bitmap* forgetSprite = assets.GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, ROSTER_SCREEN_BUTTON_FORGET_IT});

    const char* actionText = saving ? "Save it" : "Load it";

    std::vector<std::string> labels;
    for (const Entry& e : entries) labels.push_back(e.label);

    Zone actionZone("save_load_action", actionText, kActionRect);
    Zone forgetZone("save_load_forget", "Forget it", kForgetItRect);

    DialogConfig cfg;
    cfg.font = assets.GetMainFont();
    cfg.bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT};

    cfg.texts = {};
    cfg.buttons = {
        {kActionRect,   actionText,  nullptr, false, 0, ButtonDefaultTextColor,
         ButtonDefaultFillColor, ButtonDefaultBorderColor, true, {}, cfg.font},
        {kForgetItRect, "Forget it", nullptr, false, 1, ButtonDefaultTextColor,
         ButtonDefaultFillColor, ButtonDefaultBorderColor, true, {}, cfg.font},
    };
    cfg.zones = {actionZone, forgetZone};

    auto placeSprite = [](Bitmap* sprite, const SDL_Rect& at) {
        DialogBitmap db;
        db.bitmap = sprite;
        const SDL_Rect natural = sprite->GetPosition();
        db.rect = {at.x, at.y,
                   natural.w > 0 ? natural.w : at.w,
                   natural.h > 0 ? natural.h : at.h};
        db.zOrder = 0;
        return db;
    };
    if (actionSprite) cfg.bitmaps["action"] = placeSprite(actionSprite, kActionRect);
    if (forgetSprite) cfg.bitmaps["forget"] = placeSprite(forgetSprite, kForgetItRect);

    cfg.onZoneHover = [this](Zone& zone) { this->hud->SetText(zone.GetText().c_str()); };
    cfg.onButtonPressed = [this](int index) {
        if (index == 0) Commit();
        else            Leave();
    };

    ListDialogConfig listCfg;
    listCfg.labels = labels;
    listCfg.listBoxRect = kListRect;
    // The artwork leaves no room for a read-out.
    listCfg.selectedItemRect = {0, 0, 0, 0};
    listCfg.textColor = kTextColor;
    listCfg.selectionBoxColor = kSelectionBoxColor;
    listCfg.onItemSelected = [this](int) { Commit(); };

    listDialog = std::make_unique<ListDialog>(cfg, listCfg);
}

int SaveLoadScene::FindSaveNamed(const std::string& name) const {
    const std::string key = NameKey(name);
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].kind == Entry::Kind::Modern && NameKey(entries[i].label) == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void SaveLoadScene::RefreshNewSaveRow() {
    if (mode != Mode::Save || !listDialog || entries.empty()) return;
    Entry& e = entries.back();
    if (e.kind != Entry::Kind::NewSave) return;
    e.label = newSaveName;

    std::vector<std::string> labels;
    labels.reserve(entries.size());
    for (const Entry& row : entries) labels.push_back(row.label);
    // SetLabels drops the selection. Highlight the row being typed into, or the save a clashing name would overwrite.
    const int clash = FindSaveNamed(newSaveName);
    listDialog->SetLabels(labels);
    listDialog->SetSelectedIndex(clash >= 0 ? clash : static_cast<int>(entries.size()) - 1);
}

bool SaveLoadScene::LoadEntry(const Entry& e) {
    switch (e.kind) {
        case Entry::Kind::Modern:
            return state.LoadGameToml(e.path);
        case Entry::Kind::LegacyFile:
            return state.LoadSaveGame(e.path);
        case Entry::Kind::LegacyDisk2: {
            // SaveGameLoader reads from a real path.
            const auto& disk2 = state.GetAssets().GetDisk2Adf();
            AdfFileData* data = extract_adf_file_from_buffer(disk2.data(), disk2.size(),
                                                            e.adfFileName.c_str());
            if (!data) {
                LOG_INFO("SaveLoadScene: failed to extract %s from disk2", e.adfFileName.c_str());
                return false;
            }
            const auto tempPath = fs::temp_directory_path() / ("rodlegacy_sample_" + e.adfFileName);
            bool ok = false;
            {
                std::ofstream f(tempPath, std::ios::binary);
                if (f) {
                    f.write(reinterpret_cast<const char*>(data->data),
                            static_cast<std::streamsize>(data->size));
                    f.close();
                    ok = state.LoadSaveGame(tempPath);
                }
            }
            free_adf_file_data(data);
            std::error_code ec;
            fs::remove(tempPath, ec);
            return ok;
        }
        case Entry::Kind::NewSave:
            return false;
    }
    return false;
}

// The list shows each save by its player name, so a name in use is an overwrite, never a second row.
void SaveLoadScene::SaveUnderName(const std::string& rawName) {
    const std::string name = TrimSpaces(rawName);
    if (name.empty()) {
        if (hud) hud->SetText("Type a name first");
        SetActiveDialog(listDialog.get());
        return;
    }

    const int existing = FindSaveNamed(name);
    if (existing >= 0) {
        ConfirmOverwrite(entries[existing].path, name);
        return;
    }

    const fs::path dir = AssetManager::GetSavesDir() / SrGameSaveKey(state.GetAssets().GetGame());
    const std::string stem = SanitiseForFilename(name);
    fs::path target = dir / (stem + ".toml");
    // Two different names can sanitise to the same stem ("Al B" / "Al_B").
    for (int n = 2; fs::exists(target) && n < 100; ++n) {
        target = dir / (stem + "-" + std::to_string(n) + ".toml");
    }
    WriteSave(target, name);
}

void SaveLoadScene::WriteSave(const fs::path& target, const std::string& name) {
    state.SetSaveName(name);
    if (!state.SaveGameToml(target)) {
        LOG_INFO("SaveLoadScene: save '%s' FAILED", target.string().c_str());
        Configure(mode, returnTo);
        if (hud) hud->SetText("Save failed");
        return;
    }
    GoTo(returnTo);
}

void SaveLoadScene::ConfirmOverwrite(const fs::path& target, const std::string& name) {
    if (listDialog) listDialog->Hide();
    SDL_StopTextInput();

    const SDL_Rect box    = {34, 97, 252, 46};
    const SDL_Rect yesRect = {88, 123, 66, 16};
    const SDL_Rect noRect  = {166, 123, 66, 16};

    DialogConfig cfg;
    cfg.bounds = box;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{box};
    cfg.font = state.GetAssets().GetMainFont();
    cfg.texts = std::vector<DialogTextConfig>{
        {SDL_Rect{38, 103, 248, 10}, "Overwrite " + name + " ?", kTextColor, true}
    };
    // No Enter default: this throws a save away, and Enter is how you got here.
    cfg.buttons = std::vector<ButtonConfig>{
        {yesRect, "Overwrite",  nullptr, false, 0},
        {noRect,  "Forget it", nullptr, false, 1}
    };
    cfg.onButtonPressed = [this, target, name](int index) {
        if (confirmDialog) confirmDialog->Hide();
        SetActiveDialog(nullptr);
        if (index == 0) WriteSave(target, name);
        else            CancelConfirm();
    };
    cfg.onDismiss = [this] { CancelConfirm(); };

    confirmDialog = std::make_unique<Dialog>(cfg);
    SetActiveDialog(confirmDialog.get());
}

// Hidden, not destroyed: this runs from the dialog's own callback.
void SaveLoadScene::CancelConfirm() {
    if (confirmDialog) confirmDialog->Hide();
    if (mode == Mode::Save) SDL_StartTextInput();
    SetActiveDialog(listDialog.get());
}

void SaveLoadScene::Commit() {
    const int index = listDialog ? listDialog->GetSelectedIndex() : -1;
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        // ListDialog hides itself on the way here.
        SetActiveDialog(listDialog.get());
        if (hud) hud->SetText(mode == Mode::Save ? "Pick a slot" : "Pick a game");
        return;
    }
    const Entry e = entries[index];

    // Picking an existing slot saves under its name, through the same overwrite check.
    if (mode == Mode::Save) {
        SaveUnderName(e.kind == Entry::Kind::NewSave ? newSaveName : e.label);
        return;
    }

    if (!LoadEntry(e)) {
        LOG_INFO("SaveLoadScene: load '%s' FAILED", e.label.c_str());
        Configure(mode, returnTo);
        if (hud) hud->SetText("Load failed");
        return;
    }

    if (AudioManager* am = state.GetAssets().GetAudioManager()) am->RestartMusic();
    // A load resumes in the garage, not the screen it was opened from.
    GoTo(SCENE_GARAGE);
}

void SaveLoadScene::Leave() {
    GoTo(returnTo);
}

void SaveLoadScene::GoTo(SrLibGameScene scene) {
    SDL_StopTextInput();
    SetActiveDialog(nullptr);
    if (listDialog) listDialog->Hide();
    state.MoveToScene(scene);
}

void SaveLoadScene::DrawScene() {
    Scene::DrawScene();
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }
    DrawNameCursor();
}

// Drawn after the dialog so the list's colour mod doesn't tint it.
void SaveLoadScene::DrawNameCursor() {
    if (mode != Mode::Save || !listDialog || !listDialog->IsVisible() || entries.empty()) return;
    if (entries.back().kind != Entry::Kind::NewSave) return;
    if ((SDL_GetTicks64() / kCursorBlinkMs) % 2 != 0) return;

    const SDL_Rect row = listDialog->GetRowRect(static_cast<int>(entries.size()) - 1);
    if (row.w <= 0 || row.h <= 0) return;  // scrolled out of view

    SFont_Font* font = state.GetAssets().GetMainFont();
    if (!font) return;
    const int textW = newSaveName.empty()
                          ? 0
                          : SFont_TextWidth(font, newSaveName.c_str());
    const int blockW = SFont_TextWidth(font, "M");
    SDL_Rect block = {row.x + textW, row.y, blockW > 0 ? blockW : 6, row.h - 1};
    if (block.x + block.w > row.x + row.w) return;  // name filled the row

    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(renderer, kSelectionBoxColor.r, kSelectionBoxColor.g, kSelectionBoxColor.b, kSelectionBoxColor.a);
    SDL_RenderFillRect(renderer, &block);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

bool SaveLoadScene::PushEvent(SDL_Event* event) {
    // Before the base class: ListDialog would hide itself on Escape, and an unconsumed Escape quits the game.
    const bool confirming = confirmDialog && confirmDialog->IsVisible();
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        if (confirming) CancelConfirm();
        else            Leave();
        return true;
    }

    if (mode == Mode::Save && !confirming) {
        if (event->type == SDL_TEXTINPUT) {
            for (const char* p = event->text.text; *p; ++p) {
                if (newSaveName.size() >= kMaxSaveNameLength) break;
                if (*p >= 32 && *p < 127) newSaveName.push_back(*p);
            }
            RefreshNewSaveRow();
            return true;
        }
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
            if (!newSaveName.empty()) {
                newSaveName.pop_back();
                RefreshNewSaveRow();
            }
            return true;
        }
        if (event->type == SDL_KEYDOWN &&
            (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER)) {
            SaveUnderName(newSaveName);
            return true;
        }
    }

    return Scene::PushEvent(event);
}

void SaveLoadScene::OnFadeComplete() {
    Scene::OnFadeComplete();
}
