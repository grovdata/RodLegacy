#include "debug/main_debug.h"

#include <string>

#include "core/global.h"
#include "core/mouse.h"
#include "scenes/scene.h"
#include "core/state.h"
#include "race/driving.h"

extern Mouse mouse;

std::vector<DebugPanel> BuildDebugPanels(Uint64 frameDelta) {
    std::vector<DebugPanel> panels;

    DebugPanel scenes{"Scenes", {}, DebugDock::Left};
    scenes.buttons.push_back({"Garage",         [] { state.MoveToScene(SCENE_GARAGE); }});
    scenes.buttons.push_back({"Diner",          [] { state.MoveToScene(SCENE_DINER); }});
    scenes.buttons.push_back({"Driving",        [] { state.MoveToScene(SCENE_DRIVING); }});
    // The three drives, by their start segment and the building they end at:
    // 10 -> right[44] garage, 65 -> left[108] diner, 115 -> left[156] gas.
    scenes.buttons.push_back({"Cruise: garage", [] {
        state.MoveToScene(SCENE_CRUISING);
        state.GetDrivingScene()->StartCruise(10, SCENE_GARAGE);
    }});
    scenes.buttons.push_back({"Cruise: diner", [] {
        state.MoveToScene(SCENE_CRUISING);
        state.GetDrivingScene()->StartCruise(65, SCENE_DINER);
    }});
    scenes.buttons.push_back({"Cruise: gas", [] {
        state.MoveToScene(SCENE_CRUISING);
        state.GetDrivingScene()->StartCruise(115, SCENE_GAS_STATION);
    }});
    scenes.buttons.push_back({"Ending",         [] { state.MoveToScene(SCENE_ENDING); }});
    scenes.buttons.push_back({"King of County", [] { state.MoveToScene(SCENE_KING_OF_COUNTY); }});
    scenes.buttons.push_back({"Name Entry",     [] { state.MoveToScene(SCENE_NAME_ENTRY); }});
    scenes.buttons.push_back({"Game Over Menu", [] { state.MoveToScene(SCENE_GAME_OVER_MENU); }});
    scenes.buttons.push_back({"Car Debug",      [] { state.MoveToScene(SCENE_CAR_DEBUG); }});
    panels.push_back(std::move(scenes));

    DebugPanel session{"Session", {}, DebugDock::Left};
    session.rows.push_back({"frame", ""});
    session.rows.push_back({"delta", std::to_string(frameDelta) + " ms"});
    session.rows.push_back({"fps", std::to_string(frameDelta > 0 ? 1000 / frameDelta : 0)});

    session.rows.push_back({"player", ""});
    session.rows.push_back({"date", state.GetGameDateString()});
    session.rows.push_back({"hours", std::to_string(state.GetGameHours())});
    session.rows.push_back({"bankroll", "$" + std::to_string(state.GetBankroll())});
    // Both scores are car performance on the same 70..130 scale; the pool picks
    // challengers whose score is nearest the player's.
    session.rows.push_back({"car score", std::to_string(state.GetPlayerScore())});
    session.rows.push_back({"challenger car score", std::to_string(state.GetChallengerScore())});

    const Reputation& rep = state.GetReputation();
    session.rows.push_back({"races", ""});
    session.rows.push_back({"won", std::to_string(rep.races_won()) + " of " +
                                   std::to_string(rep.races_completed)});
    session.rows.push_back({"drag", std::to_string(rep.drag_wins) + " won, rep " +
                                    std::to_string(rep.drag_rep)});
    session.rows.push_back({"road", std::to_string(rep.road_wins) + " won, rep " +
                                    std::to_string(rep.road_rep)});

    session.rows.push_back({"input", ""});
    for (const DebugRow& row : mouse.GetGamepadDebugRows()) {
        session.rows.push_back(row);
    }
    panels.push_back(std::move(session));

    Scene* scene = state.GetCurrentScene();
    if (scene) {
        for (DebugPanel& panel : scene->GetDebugPanels()) {
            panels.push_back(std::move(panel));
        }
    }
    return panels;
}

void RemapMouseToGameViewport(SDL_Event& event, const SDL_Rect& vp) {
    if (vp.w <= 0 || vp.h <= 0) {
        return;
    }
    auto mapX = [&](int x) {
        return static_cast<int>((static_cast<int64_t>(x - vp.x) * GAME_WIDTH) / vp.w);
    };
    auto mapY = [&](int y) {
        return static_cast<int>((static_cast<int64_t>(y - vp.y) * GAME_HEIGHT) / vp.h);
    };
    switch (event.type) {
        case SDL_MOUSEMOTION:
            event.motion.x = mapX(event.motion.x);
            event.motion.y = mapY(event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            event.button.x = mapX(event.button.x);
            event.button.y = mapY(event.button.y);
            break;
        default:
            break;
    }
}
