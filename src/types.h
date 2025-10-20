#pragma once
#include <string>
#include "parsers/libp.h"
#include <SDL.h>

class IDrawable {
    protected:
        bool visible;
    public:
        virtual ~IDrawable() = default;
        virtual void Draw() = 0;
        virtual void SetVisible(bool visible) = 0;
};

struct DrawableEntry {
    std::string id;
    std::shared_ptr<IDrawable> drawable;
};

class IDrawableContainer {
    protected:
        std::vector<DrawableEntry> drawables;

    public:
        virtual ~IDrawableContainer() = default;

        void AddDrawable(const std::string& id, const std::shared_ptr<IDrawable> &drawable) {
            drawables.push_back({id, drawable});
        }

        void RemoveDrawable(const std::string& id) {
            drawables.erase(
                std::remove_if(drawables.begin(), drawables.end(),
                    [&id](const DrawableEntry& entry) { return entry.id == id; }),
                drawables.end()
            );
        }

        void ClearDrawables() {
            drawables.clear();
        }

        std::shared_ptr<IDrawable> GetDrawable(const std::string& id) {
            for (const auto& entry : drawables) {
                if (entry.id == id) {
                    return entry.drawable;
                }
            }
            return nullptr;
        }

        void DrawAll() {
            for (const auto& entry : drawables) {
                if (entry.drawable) {
                    entry.drawable->Draw();
                }
            }
        }

        void SetAllVisible(bool visible) {
            for (const auto& entry : drawables) {
                if (entry.drawable) {
                    entry.drawable->SetVisible(visible);
                }
            }
        }

        void Clear() {
            drawables.clear();
        }

        size_t GetDrawableCount() const {
            return drawables.size();
        }
};

struct CarModel {
    // For custom cars
    std::string id;
    std::string filename;

    // Currently not known where this is defined
    int baseSpeed;
    int baseAcceleration;
    
    // From SR_data
    int year;
    std::string name;
    CarVendor_t vendor;
    BitmapOffset_t offsets;
    
    // From lib.p
    int width, height;    
    int index;
    
    // Modification constraints (will be set by CarModelConfigLoader)
    struct {
        bool canChopRoof = true;
        bool canStripBumpers = true;
        bool canAddDecal = true;
        std::string constraintReason = "";
    } constraints;
    
    // Compatible engines (will be set by CarModelConfigLoader)
    std::vector<CarEngine_t> compatibleEngines;

    void Log() const {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, 
            "CarModel: id=%s, filename=%s, baseSpeed=%d, year=%d, name=%s, vendor=%d, width=%d, height=%d, index=%d",
            id.c_str(), filename.c_str(), baseSpeed, year, name.c_str(), vendor, width, height, index);
    }
};

bool operator<(const CarModel &a, const CarModel &b);