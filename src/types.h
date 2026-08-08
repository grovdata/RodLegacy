#pragma once
#include "log.h"
#include <algorithm>
#include <memory>
#include <string>
#include "parsers/libp.h"
#include "car_set.h"
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
    IDrawable* drawable;  // Non-owning; ownership is elsewhere (e.g. AssetManager or scene)
};

class IDrawableContainer {
    protected:
        std::vector<DrawableEntry> drawables;

    public:
        virtual ~IDrawableContainer() = default;

        void AddDrawable(const std::string& id, IDrawable* drawable) {
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

        IDrawable* GetDrawable(const std::string& id) {
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
    // Pool key. For the Amiga set this is the lib.p car body asset; other sets
    // shift theirs clear of that range so the key stays unique (see
    // CarData::ReadCarSet). Use sprites.body to draw, not this.
    int index;

    // Which car set the model came from, and where its art sits in that set's
    // lib. Both are resolved when the model is built, so nothing downstream
    // needs to know a set's sprite-numbering conventions.
    int        carSet = kAmigaCarSet;
    CarSprites sprites;


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
        LOG_INFO(
            "CarModel: id=%s, filename=%s, baseSpeed=%d, year=%d, name=%s, vendor=%d, width=%d, height=%d, index=%d",
            id.c_str(), filename.c_str(), baseSpeed, year, name.c_str(), vendor, width, height, index);
    }
};

bool operator<(const CarModel &a, const CarModel &b);

// Whether a car can take each modification. Answered from the resolved sprite
// numbers rather than a lookup keyed on CarModel::index, which is only a lib.p
// asset for the Amiga set.
inline bool CarModelCanChopRoof(const CarModel &model) { return model.sprites.choppedRoof >= 0; }
inline bool CarModelCanStripBumpers(const CarModel &model) {
    return model.sprites.rearBumperStripped >= 0 || model.sprites.frontBumperStripped >= 0;
}

// Fills in sprites for a car in the Amiga stock set, where the chopped roof
// and bumper patches sit at fixed offsets from the body asset.
CarSprites AmigaCarSprites(int bodyAsset);