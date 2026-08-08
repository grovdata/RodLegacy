#pragma once
#include "log.h"
#include <algorithm>
#include <memory>
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
    int index;

    // Which lib the car's art comes from. The stock 25 are Amiga lib.p, where
    // a car's chopped roof and bumper patches are always at index+1..index+3
    // and the sprite is recoloured through the car palette. Street Rod SE
    // cars come out of the data disk's LIB2, which scatters (and sometimes
    // shares) the variants, so they are named per car in dosSprites instead.
    SrLibFile spriteSource = SrLibFileAmigaLibP;
    // Only meaningful when spriteSource is SrLibFileSrseLib2. -1 = no variant.
    DosCarSprites dosSprites = {-1, -1, -1, -1, -1};


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

// Which LIB sprite holds each part of a car, in whichever lib the model draws
// from, and -1 when the car has no such variant. Amiga lib.p cars keep their
// chopped roof and bumper patches at index+1..index+3 and answer through the
// CarQuery tables; Street Rod SE cars name theirs per car in dosSprites.
// Anything asking "can this car chop its roof / strip its bumpers" should go
// through these rather than casting CarModel::index to an AmigaLibAsset - an
// SE index is deliberately outside lib.p's range and will not look up.
bool CarModelIsSrse(const CarModel &model);
int CarBodySprite(const CarModel &model);
int CarChoppedRoofSprite(const CarModel &model);
int CarRearBumperSprite(const CarModel &model);
int CarFrontBumperSprite(const CarModel &model);
inline bool CarModelCanChopRoof(const CarModel &model) { return CarChoppedRoofSprite(model) >= 0; }
inline bool CarModelCanStripBumpers(const CarModel &model) {
    return CarRearBumperSprite(model) >= 0 || CarFrontBumperSprite(model) >= 0;
}