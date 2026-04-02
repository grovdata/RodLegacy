#include "log.h"
#include "king_of_county.h"
#include "parsers/libp.h"
#include "SFont.h"
#include <algorithm>

// Layout constants (320x200 screen)
static constexpr int TABLE_START_Y    = 58;
static constexpr int ROW_HEIGHT       = 11;
static constexpr int NAME_X           = 46;
static constexpr int SCORE_RIGHT_X    = 180;  // right edge for score column
static constexpr uint32_t COLOR_NORMAL    = 0xFFFFFFFF;  // white
static constexpr uint32_t COLOR_HIGHLIGHT = 0xFF00FFFF;  // yellow

KingOfCounty::KingOfCounty(bool wonGame)
    : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, KING_STREET_RODDERS}),
      wonGame(wonGame) {}

void KingOfCounty::Init(bool debugMode) {
    Scene::Init(debugMode);

    auto entries = state.LoadHallDat();

    if (wonGame) {
        int playerScore = state.CalculateScore();
        State::HallEntry playerEntry{ state.GetPlayerName(), playerScore };

        // Insert at correct ascending position
        auto it = std::lower_bound(entries.begin(), entries.end(), playerEntry,
            [](const State::HallEntry& a, const State::HallEntry& b) {
                return a.score < b.score;
            });
        newEntryIndex = (int)(it - entries.begin());
        entries.insert(it, playerEntry);

        // Trim to 10
        if ((int)entries.size() > 10) {
            entries.resize(10);
            if (newEntryIndex >= 10) newEntryIndex = -1;  // pushed off table
        }

        state.SaveHallDat(entries);
        LOG_INFO("KingOfCounty: player score=%d inserted at index=%d", playerScore, newEntryIndex);
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
        row.name.SetText(entries[i].name);

        std::string scoreStr = std::to_string(entries[i].score);
        int scoreWidth = SFont_TextWidth(font, scoreStr.c_str());
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
        state.MoveToScene(SCENE_DINER);
        return true;
    }
    return Scene::PushEvent(event);
}

void KingOfCounty::Action(ZoneId /*zone*/) {}
