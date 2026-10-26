#include "core/log.h"
#include "race/king_of_county.h"
#include "parsers/libp.h"
#include "gfx/SFont.h"
#include <algorithm>

static constexpr int TABLE_START_Y    = 58;
static constexpr int ROW_HEIGHT       = 11;
static constexpr int NAME_X           = 46;
static constexpr int SCORE_RIGHT_X    = 180;
static constexpr int NAME_GAP         = 8;
static constexpr uint32_t COLOR_NORMAL    = 0xFFFFFFFF;
static constexpr uint32_t COLOR_HIGHLIGHT = 0xFFFFFF00;  // yellow

KingOfCounty::KingOfCounty(bool wonGame)
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, KING_STREET_RODDERS}),
      wonGame(wonGame) {}

void KingOfCounty::Init(bool debugMode) {
    Scene::Init(debugMode);

    auto entries = state.LoadHallDat();

    if (wonGame) {
        int playerScore = state.CalculateScore();
        State::HallEntry playerEntry{ state.GetPlayerName().substr(0, 15), playerScore };

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
            state.SaveHallDat(entries);
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
        bool highlight = (i == newEntryIndex);
        uint32_t color = highlight ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        Row row;
        row.highlight = highlight;

        row.name.Init(font, NAME_X, y, color);
        std::string scoreStr = std::to_string(entries[i].score) + " hr";
        int scoreWidth = SFont_TextWidth(font, scoreStr.c_str());
        std::string name = entries[i].name;
        while (name.size() > 1 && NAME_X + SFont_TextWidth(font, name.c_str()) > SCORE_RIGHT_X - scoreWidth - NAME_GAP) {
            name.pop_back();
        }
        row.name.SetText(name);
        row.score.Init(font, SCORE_RIGHT_X - scoreWidth, y, color);
        row.score.SetText(scoreStr);

        rows.push_back(std::move(row));
    }
}

void KingOfCounty::DrawScene() {
    Scene::DrawScene();
    for (auto& row : rows) {
        row.name.Draw();
        row.score.Draw();
    }
}

bool KingOfCounty::PushEvent(SDL_Event* event) {
    if (event->type == SDL_KEYDOWN || event->type == SDL_MOUSEBUTTONDOWN) {
        state.MoveToScene(SCENE_GAME_OVER_MENU);
        return true;
    }
    return Scene::PushEvent(event);
}

