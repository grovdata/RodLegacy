#include "scene_layout.h"
#include "log.h"
#include "include/toml.hpp"
#include <sstream>

std::vector<Zone> SceneLayout::Load(const char* tomlText) {
    std::vector<Zone> result;
    try {
        std::istringstream ss(tomlText);
        auto tbl = toml::parse(ss);

        if (!tbl.contains("zones")) return result;

        for (auto& item : tbl.at("zones").as_array()) {
            std::string id   = toml::find<std::string>(item, "id");
            std::string text = toml::find<std::string>(item, "text");
            auto rv          = toml::find<std::vector<int>>(item, "rect");
            SDL_Rect r       = {rv[0], rv[1], rv[2], rv[3]};
            result.emplace_back(std::move(id), std::move(text), r);
        }
    } catch (const std::exception& e) {
        LOG_WARN("SceneLayout::Load failed: %s", e.what());
    }
    return result;
}
