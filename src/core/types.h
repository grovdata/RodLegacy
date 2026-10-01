/*
 * RodLegacy - Shared types: CarModel and its sprite lookups, and the IDrawable interface.
 *
 * Created: 2024-02-03
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include "core/log.h"
#include <string>
#include "parsers/libp.h"
#include "car/car_set.h"
#include <SDL.h>

class IDrawable {
    protected:
        bool visible;
    public:
        virtual ~IDrawable() = default;
        virtual void Draw() = 0;
        virtual void SetVisible(bool visible) = 0;
};

struct CarModel {
    // Currently not known where this is defined
    int baseSpeed;
    int baseAcceleration;
    
    int year;
    // List price in dollars, 0 = never for sale.
    int price = 0;
    std::string name;
    CarVendor_t vendor;
    BitmapOffset_t offsets;
    
    int width, height;
    // Pool key: the lib.p body asset for the Amiga set, shifted clear of it for other sets. Draw with sprites.body.
    int index;

    int        carSet = kAmigaCarSet;
    CarSprites sprites;


    struct {
        bool canChopRoof = true;
        bool canStripBumpers = true;
        bool canAddDecal = true;
        std::string constraintReason = "";
    } constraints;
    
    std::vector<CarEngine_t> compatibleEngines;

    void Log() const {
        LOG_INFO(
            "CarModel: baseSpeed=%d, year=%d, name=%s, vendor=%d, width=%d, height=%d, index=%d",
            baseSpeed, year, name.c_str(), vendor, width, height, index);
    }
};

bool operator<(const CarModel &a, const CarModel &b);

inline bool CarModelCanChopRoof(const CarModel &model) { return model.sprites.choppedRoof >= 0; }
inline bool CarModelCanStripBumpers(const CarModel &model) {
    return model.sprites.rearBumperStripped >= 0 || model.sprites.frontBumperStripped >= 0;
}

CarSprites AmigaCarSprites(int bodyAsset);