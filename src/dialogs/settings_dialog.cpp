#include "dialogs/settings_dialog.h"

#include "core/assets.h"
#include "core/embedded_loader.h"
#include "core/global.h"
#include "core/log.h"
#include "core/state.h"
#include "race/race_sim.h"
#include "ui/main_font.h"

#include <vector>

extern "C" {
    #include "embedded_resources.h"
}

namespace {

// The settings with no other UI. Each one is a plain bool the game reads
// directly, so the row edits it in place and then persists the lot.
//
// Debug-only settings do not belong here - the zone overlay toggle lives in the
// debug panel (Scene::GetDebugPanels), which is where the rest of the debug
// switches are.
//
// `defaultOn` is what the game does with no config.txt at all, which is what
// the Defaults button restores - every one of these is a deviation from the
// original, so they all default off.
struct Setting {
    const char* label;
    bool& (AssetManager::*ref)();
    bool defaultOn;
    // Settings the game reads straight off the bool leave this null. One that
    // another subsystem keeps its own copy of uses it to push the new value.
    void (*onChange)(bool);
};

const Setting kSettings[] = {
    {"Hill occlusion",      &AssetManager::GetHillOcclusionRef,    false, nullptr},
    {"Extra animations",    &AssetManager::GetExtraAnimationsRef,  false, nullptr},
    {"Analog steering",     &AssetManager::GetAnalogSteeringRef,   false, nullptr},
    {"Skip intro",          &AssetManager::GetSkipIntroRef,        false, nullptr},
    {"Purist tune scoring", &AssetManager::GetPuristScoringRef,    false,
     [](bool on) { srrace::SetPuristScoring(on); }},
};
constexpr int kSettingCount = sizeof(kSettings) / sizeof(kSettings[0]);

SDL_Surface* g_checkOn = nullptr;
SDL_Surface* g_checkOff = nullptr;

void LoadCheckboxes() {
    if (!g_checkOn) {
        g_checkOn = LoadSurfaceFromMemory(embedded_checkbox_on_png,
                                          embedded_checkbox_on_png_len);
    }
    if (!g_checkOff) {
        g_checkOff = LoadSurfaceFromMemory(embedded_checkbox_off_png,
                                           embedded_checkbox_off_png_len);
    }
    if (!g_checkOn || !g_checkOff) {
        LOG_WARN("settings_dialog: failed to decode the checkbox art");
    }
}

bool& ValueOf(int i) {
    return (state.GetAssets().*(kSettings[i].ref))();
}

SDL_Surface* SwatchFor(int i) {
    return ValueOf(i) ? g_checkOn : g_checkOff;
}

// Writes a row's value and lets whoever else holds a copy of it know.
void ApplyValue(int i, bool on) {
    ValueOf(i) = on;
    if (kSettings[i].onChange) kSettings[i].onChange(on);
}

}  // namespace

void FreeSettingsIcons() {
    if (g_checkOn) {
        SDL_FreeSurface(g_checkOn);
        g_checkOn = nullptr;
    }
    if (g_checkOff) {
        SDL_FreeSurface(g_checkOff);
        g_checkOff = nullptr;
    }
}

std::unique_ptr<ListDialog> MakeSettingsDialog(SFont_Font* font,
                                               std::function<void()> onBack) {
    LoadCheckboxes();

    std::vector<std::string> labels;
    std::vector<SDL_Surface*> swatches;
    for (int i = 0; i < kSettingCount; ++i) {
        labels.push_back(kSettings[i].label);
        swatches.push_back(SwatchFor(i));
    }

    // Same geometry as the key bindings screen so the two read as a pair.
    const int kRowHeight = 18;
    const int kScrollW = 15;
    const int kListTop = 20;
    const int kListBottom = GAME_HEIGHT - 20;
    const int kVisibleRows = (kListBottom - kListTop) / kRowHeight;

    SDL_Rect bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT};
    SDL_Rect listBox = {6, kListTop, GAME_WIDTH - 12 - kScrollW - 4,
                        kVisibleRows * kRowHeight};

    const int scrollX = listBox.x + listBox.w + 4;
    SDL_Rect scrollTopRect    = {scrollX, listBox.y, kScrollW, kScrollW};
    SDL_Rect scrollUpRect     = {scrollX, listBox.y + 17, kScrollW, kScrollW};
    SDL_Rect scrollDownRect   = {scrollX, listBox.y + listBox.h - 32, kScrollW, kScrollW};
    SDL_Rect scrollBottomRect = {scrollX, listBox.y + listBox.h - 15, kScrollW, kScrollW};

    const int buttonW = 66, buttonH = 16;
    const int buttonY = GAME_HEIGHT - 18;
    SDL_Rect defaultsRect = {GAME_WIDTH / 2 - buttonW - 8, buttonY, buttonW, buttonH};
    SDL_Rect backRect     = {GAME_WIDTH / 2 + 8, buttonY, buttonW, buttonH};

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{bounds};
    cfg.font = font;
    cfg.texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10},
                         "Settings", {0x00, 0x00, 0x00, 0xFF}, true},
    };
    cfg.lines = {{bounds.x + 4, listBox.y - 2, bounds.w - 8, 1}};

    // The row and button callbacks both need the dialog, but they have to be in
    // its config before it is built, so they reach it through a holder filled in
    // once it exists.
    auto self = std::make_shared<ListDialog*>(nullptr);

    auto toggleRow = [self](int row) {
        if (!*self || row < 0 || row >= kSettingCount) return;
        const bool value = !ValueOf(row);
        ApplyValue(row, value);
        (*self)->SetSwatch(row, SwatchFor(row));
        state.GetAssets().SaveSettings();
        LOG_INFO("Settings: %s %s", kSettings[row].label, value ? "on" : "off");
    };

    ButtonConfig defaultsButton = {defaultsRect, "Defaults", nullptr, false, 0};
    ButtonConfig backButton = {backRect, "Forget it", nullptr, true, 1};
    backButton.cancelChoice = true;
    cfg.buttons = {defaultsButton, backButton};

    cfg.onButtonPressed = [self, onBack](int index) {
        switch (index) {
            case 0:
                for (int i = 0; i < kSettingCount; ++i) {
                    ApplyValue(i, kSettings[i].defaultOn);
                    if (*self) (*self)->SetSwatch(i, SwatchFor(i));
                }
                state.GetAssets().SaveSettings();
                LOG_INFO("Settings: restored defaults");
                break;
            case 1:
                if (onBack) onBack();
                break;
        }
    };

    ListDialogConfig listCfg;
    listCfg.labels = labels;
    listCfg.swatches = swatches;
    listCfg.listBoxRect = listBox;
    listCfg.rowHeight = kRowHeight;
    listCfg.scrollButtons = {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect};
    // Clicking a row flips its checkbox there and then; nothing to confirm, so
    // no selection highlight and no separate action button.
    listCfg.activateOnClick = true;
    listCfg.onItemSelected = toggleRow;

    auto dialog = std::make_unique<ListDialog>(cfg, listCfg);
    *self = dialog.get();
    return dialog;
}
