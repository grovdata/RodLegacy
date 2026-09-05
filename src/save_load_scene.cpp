#include "save_load_scene.h"

#include "state.h"
#include "assets.h"
#include "game_clock.h"
#include "save_file.h"
#include "save_loader.h"
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

// Trims the space padding the .sav header stores names with (save_game.md).
std::string TrimTrailingSpaces(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::string LegacyLabel(const std::string& filename, const std::string& rawSaveName) {
    const std::string trimmed = TrimTrailingSpaces(rawSaveName);
    if (trimmed.empty()) return filename;
    return trimmed;
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

} // namespace

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
    RebuildList();
    const Uint64 t1 = SDL_GetTicks64();
    BuildDialog();
    const Uint64 t2 = SDL_GetTicks64();
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

    // RodLegacy's own saves, offered by both modes - loadable, and in save
    // mode each row is a slot to overwrite.
    for (const auto& path : ListSaveFiles(AssetManager::GetSavesDir())) {
        Entry e;
        e.kind = Entry::Kind::Modern;
        e.path = path;
        e.label = path.stem().string();
        try {
            const SaveSummary s = PeekSave(path);
            e.label = s.playerName;
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
        e.label = state.GetPlayerName();
        entries.push_back(e);
        return;
    }

    // Original .sav files, load-only: loose in the game directory first, then
    // the samples shipped on disk2.
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

    cfg.texts = {
        DialogTextConfig{SDL_Rect{kListRect.x, kListRect.y - 11, kListRect.w, 10},
                         saving ? "Save which slot?" : "Load which game?",
                         kTextColor, true}
    };
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
    // No centred read-out: the list highlights the current pick in place, and
    // the artwork leaves no room under the box for a second copy of it.
    listCfg.selectedItemRect = {0, 0, 0, 0};
    listCfg.textColor = kTextColor;
    listCfg.selectionBoxColor = kSelectionBoxColor;
    listCfg.onItemSelected = [this](int) { Commit(); };

    listDialog = std::make_unique<ListDialog>(cfg, listCfg);
}

bool SaveLoadScene::LoadEntry(const Entry& e) {
    switch (e.kind) {
        case Entry::Kind::Modern:
            return state.LoadGameToml(e.path);
        case Entry::Kind::LegacyFile:
            return state.LoadSaveGame(e.path);
        case Entry::Kind::LegacyDisk2: {
            // SaveGameLoader reads from a real path, so the ADF member has to
            // land on disk first.
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

void SaveLoadScene::Commit() {
    const int index = listDialog ? listDialog->GetSelectedIndex() : -1;
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        // Nothing picked yet. ListDialog::OnItemSelected hides itself on the
        // way here, so put it back rather than leaving a bare background.
        SetActiveDialog(listDialog.get());
        if (hud) hud->SetText(mode == Mode::Save ? "Pick a slot" : "Pick a game");
        return;
    }
    const Entry e = entries[index];

    bool ok;
    if (mode == Mode::Save) {
        fs::path target = e.path;
        if (e.kind == Entry::Kind::NewSave) {
            const fs::path dir = AssetManager::GetSavesDir();
            const std::string stem = SanitiseForFilename(state.GetPlayerName());
            target = dir / (stem + ".toml");
            for (int n = 2; fs::exists(target) && n < 100; ++n) {
                target = dir / (stem + "-" + std::to_string(n) + ".toml");
            }
        }
        ok = state.SaveGameToml(target);
    } else {
        ok = LoadEntry(e);
    }

    if (!ok) {
        LOG_INFO("SaveLoadScene: %s '%s' FAILED",
                 mode == Mode::Save ? "save" : "load", e.label.c_str());
        // Rebuild so a half-written or unreadable slot is reflected, and stay
        // put so the player can pick something else.
        Configure(mode, returnTo);
        if (hud) hud->SetText(mode == Mode::Save ? "Save failed" : "Load failed");
        return;
    }

    // A finished load resumes in the garage - the hub the loaded game belongs
    // in - rather than wherever the screen was opened from, which for the
    // name-entry route is a new-game screen the player has left behind.
    GoTo(mode == Mode::Load ? SCENE_GARAGE : returnTo);
}

void SaveLoadScene::Leave() {
    GoTo(returnTo);
}

void SaveLoadScene::GoTo(SrLibGameScene scene) {
    SetActiveDialog(nullptr);
    if (listDialog) listDialog->Hide();
    state.MoveToScene(scene);
}

void SaveLoadScene::DrawScene() {
    Scene::DrawScene();
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }
}

bool SaveLoadScene::PushEvent(SDL_Event* event) {
    // Ahead of the base class, which hands Escape to the dialog - ListDialog
    // would hide itself and leave the screen empty, and an unconsumed Escape
    // quits the game outright (main.cpp).
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        Leave();
        return true;
    }
    return Scene::PushEvent(event);
}

void SaveLoadScene::OnFadeComplete() {
    Scene::OnFadeComplete();
}
