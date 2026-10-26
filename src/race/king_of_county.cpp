#include "core/log.h"
#include "race/king_of_county.h"
#include "parsers/libp.h"
#include "gfx/SFont.h"
#include <algorithm>

static constexpr int TABLE_START_Y    = 58;
static constexpr int ROW_HEIGHT       = 11;
static constexpr int NAME_X           = 46;
static constexpr int SCORE_RIGHT_X    = 185;
static constexpr int HR_X             = 191;
static constexpr int NAME_GAP         = 8;
static constexpr int MAX_NAME_LEN     = 15;
static constexpr Uint64 CARET_BLINK_MS = 500;
static constexpr uint32_t COLOR_TEXT  = 0xFF222222;

KingOfCounty::KingOfCounty(bool wonGame)
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, KING_STREET_RODDERS}),
      wonGame(wonGame) {}

void KingOfCounty::Init(bool debugMode) {
    Scene::Init(debugMode);

    auto entries = state.LoadHallDat();

    if (wonGame) {
        int playerScore = state.CalculateScore();
        State::HallEntry playerEntry{ state.GetPlayerName().substr(0, MAX_NAME_LEN), playerScore };

        auto it = std::lower_bound(entries.begin(), entries.end(), playerEntry,
            [](const State::HallEntry& a, const State::HallEntry& b) {
                return a.score < b.score;
            });
        newEntryIndex = (int)(it - entries.begin());
        entries.insert(it, playerEntry);

        if ((int)entries.size() > 10) {
            entries.resize(10);
            if (newEntryIndex >= 10) newEntryIndex = -1;
        }

        if (newEntryIndex >= 0) {
            editing = true;
            editName = playerEntry.name;
            LOG_INFO("KingOfCounty: player score=%d inserted at index=%d", playerScore, newEntryIndex);
        } else {
            LOG_INFO("KingOfCounty: player score=%d did not qualify for top 10", playerScore);
        }
    }

    SFont_Font* font = state.GetAssets().GetMainFont();

    rows.clear();
    int count = std::min((int)entries.size(), 10);
    for (int i = 0; i < count; i++) {
        int y = TABLE_START_Y + i * ROW_HEIGHT;
        const uint32_t color = COLOR_TEXT;

        Row row;
        row.name.Init(font, NAME_X, y, color);
        std::string scoreStr = std::to_string(entries[i].score);
        int scoreWidth = SFont_TextWidth(font, scoreStr.c_str());
        std::string name = entries[i].name;
        while (name.size() > 1 && NAME_X + SFont_TextWidth(font, name.c_str()) > SCORE_RIGHT_X - scoreWidth - NAME_GAP) {
            name.pop_back();
        }
        row.name.SetText(name);
        row.score.Init(font, SCORE_RIGHT_X - scoreWidth, y, color);
        row.score.SetText(scoreStr);
        row.hr.Init(font, HR_X, y, color);
        row.hr.SetText("hr");

        rows.push_back(std::move(row));
    }
    allEntries = std::move(entries);
    shownName.clear();
}

void KingOfCounty::OnSceneEnter() {
    Scene::OnSceneEnter();
    caretStart = GameTicks();
    if (editing) SDL_StartTextInput();
}

void KingOfCounty::OnSceneExit() {
    SDL_StopTextInput();
    Scene::OnSceneExit();
}

void KingOfCounty::CommitName() {
    editing = false;
    SDL_StopTextInput();
    allEntries[newEntryIndex].name = editName;
    state.SaveHallDat(allEntries);
    rows[newEntryIndex].name.SetText(editName);
}

void KingOfCounty::DrawScene() {
    Scene::DrawScene();
    if (editing) {
        const bool caret = ((GameTicks() - caretStart) / CARET_BLINK_MS) % 2 == 0;
        const std::string text = editName + (caret ? "_" : "");
        if (text != shownName) {
            shownName = text;
            rows[newEntryIndex].name.SetText(text);
        }
    }
    for (auto& row : rows) {
        row.name.Draw();
        row.score.Draw();
        row.hr.Draw();
    }
}

bool KingOfCounty::PushEvent(SDL_Event* event) {
    if (editing) {
        if (event->type == SDL_TEXTINPUT) {
            SFont_Font* font = state.GetAssets().GetMainFont();
            const int maxWidth = SCORE_RIGHT_X - 4 * 6 - NAME_X - NAME_GAP;
            for (const char* p = event->text.text; *p; ++p) {
                const std::string next = editName + *p;
                if (*p >= 32 && *p < 127 && (int)next.size() <= MAX_NAME_LEN &&
                    SFont_TextWidth(font, (next + "_").c_str()) <= maxWidth) {
                    editName = next;
                }
            }
            caretStart = GameTicks();
        } else if (event->type == SDL_KEYDOWN) {
            const SDL_Keycode key = event->key.keysym.sym;
            if (key == SDLK_BACKSPACE && !editName.empty()) {
                editName.pop_back();
                caretStart = GameTicks();
            } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                CommitName();
            }
        }
        return true;
    }
    if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {
        state.MoveToScene(SCENE_GAME_OVER_MENU);
        return true;
    }
    return Scene::PushEvent(event);
}

