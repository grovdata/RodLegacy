#include "core/log.h"
#include "car/cardata.h"
#include "core/utils.h"
#include <fstream>
#include <SDL.h>
#include "parsers/libp.h"
#include "car/car_set.h"

CarData::CarData() {
}

CarData::~CarData() {

}


CarModel CarData::GetModelFromIndex(int index) {
    for (auto m: allCarModels) {
        bool equals = m.index == index;
        if (equals) {
            LOG_INFO("Found model %s\n", m.name.c_str());
            return m;
        }
    }
    LOG_WARN("No model found for id %d", index);
    return CarModel();
}

int CarData::ReadCarSetAmiga(int carSet, const std::vector<uint8_t>& srData,
                             const std::pair<int, std::vector<BitmapEntry_t>>& bitmaps) {
    const CarSetDef& def = GetCarSetDefs()[carSet];
    const bool isSr2 = def.game == SrGame::SR2;

    const std::vector<SrCarSpecEntry> specs =
        isSr2 ? dp.ParseCarSpecTableSR2(srData) : dp.ParseCarSpecTable(srData);
    const std::vector<CarSpriteRefs> sprites = dp.ParseCarSpritesAmiga(srData, def.game);
    if (specs.empty() || sprites.empty()) {
        LOG_WARN("Car set '%s': data file too short to parse, no cars added", def.id.c_str());
        return 0;
    }

    int metadataCount = 0;
    CarMetadata_t* metadata =
        isSr2 ? dp.SrLibDataReadDataFromBufferSR2(srData.data(), srData.size(), &metadataCount)
              : dp.SrLibDataReadDataFromBuffer(srData.data(), srData.size(), &metadataCount);
    if (!metadata) {
        LOG_WARN("Car set '%s': classifieds unreadable, no cars added", def.id.c_str());
        return 0;
    }

    int added = 0;
    for (int i = 0; i < metadataCount && i < (int)sprites.size(); i++) {
        // The King's car has no classified ad and is never bought, so it gets
        // no model - the same slot Street Rod's reader always left out.
        if (i < (int)specs.size() && !specs[i].ForSale()) continue;
        const int bodySprite = sprites[i].body;
        if (bodySprite < 0 || bodySprite >= bitmaps.first) {
            LOG_WARN("Car set '%s': car %d names body sprite %d, outside the %d-entry lib.p - skipping",
                     def.id.c_str(), i, bodySprite, bitmaps.first);
            continue;
        }
        const BitmapEntry_t& bitmap = bitmaps.second[bodySprite];

        CarModel model;
        // Street Rod's own cars keep their raw lib.p asset id as the pool key, because original .SAV files
        // store it.
        model.index = carSet == kAmigaCarSet ? bodySprite
                                             : carSet * HOT_DATA_SPRITE_BASE + bodySprite;
        model.carSet = carSet;
        model.sprites.body = bodySprite;
        model.sprites.dashboard = sprites[i].dashboard;
        model.sprites.bodyStyle = sprites[i].bodyStyle;
        model.sprites.choppedRoof = sprites[i].choppedRoof;
        model.sprites.airScoop = sprites[i].airScoop;
        model.sprites.rearBumperStripped = sprites[i].rearBumperStripped;
        model.sprites.frontBumperStripped = sprites[i].frontBumperStripped;
        model.name = metadata[i].classified.carName ? metadata[i].classified.carName : "";
        model.year = metadata[i].classified.year;
        model.price = metadata[i].classified.price;
        model.offsets = metadata[i].offsets;
        // No data file carries a wheel Y; the wheel cell is bottom-aligned to
        // the body sprite instead.
        model.offsets.offsetYTires = CarSetWheelY(carSet, bitmap.height);
        metadata[i].offsets.offsetYTires = model.offsets.offsetYTires;
        model.width = bitmap.width;
        model.height = bitmap.height;
        // FIXME Speed is unknown
        model.baseSpeed = 100;
        switch (i < (int)specs.size() ? specs[i].Brand() : -1) {
            case 0:  model.vendor = CarVendorGM; break;
            case 1:  model.vendor = CarVendorFord; break;
            case 2:  model.vendor = CarVendorChrysler; break;
            default: model.vendor = CarVendorGeneric; break;
        }

        metadataByIndex[model.index] = metadata[i];
        allCarModels.insert(model);
        added++;
    }

    // Only the array.
    free(metadata);

    LOG_INFO("Car set '%s': added %d cars", def.id.c_str(), added);
    return added;
}

CarMetadata_t CarData::GetCarMetadata(int carIndex) {
    auto found = metadataByIndex.find(carIndex);
    if (found != metadataByIndex.end()) {
        return found->second;
    }
    LOG_INFO("Warning: GetCarMetadata carIndex %d not known (%zu entries), returning empty metadata",
             carIndex, metadataByIndex.size());
    CarMetadata_t empty = {};
    return empty;
}

int CarData::ReadCarSet(int carSet, const std::vector<uint8_t>& hotData,
                        const std::pair<int, std::vector<DOSBitmapEntry_t>>& bitmaps) {
    const std::string& setId = GetCarSetDefs()[carSet].id;
    const std::vector<SrCarSpecEntry> specs = dp.ParseCarSpecTableDOS(hotData);
    const std::vector<CarSpriteRefs> sprites = dp.ParseCarSpritesDOS(hotData);
    if (specs.empty() || sprites.empty()) {
        LOG_WARN("Car set '%s': data file too short to parse, no cars added", setId.c_str());
        return 0;
    }

    int metadataCount = 0;
    CarMetadata_t* metadata = dp.SrLibDataReadDataFromBufferDOS(hotData.data(), hotData.size(), &metadataCount);
    if (!metadata) {
        LOG_WARN("Car set '%s': classifieds unreadable, no cars added", setId.c_str());
        return 0;
    }

    int added = 0;
    for (int i = 0; i < metadataCount; i++) {
        const int bodySprite = sprites[i].body;
        if (bodySprite < 0 || bodySprite >= bitmaps.first) {
            LOG_WARN("Car set '%s': car %d names body sprite %d, outside the %d-entry lib - skipping",
                     setId.c_str(), i, bodySprite, bitmaps.first);
            continue;
        }
        const DOSBitmapEntry_t& bitmap = bitmaps.second[bodySprite];

        CarModel model;
        // DOS body sprite numbers overlap lib.p's car assets, so shift each set clear to keep CarModel::index unique.
        model.index = carSet * HOT_DATA_SPRITE_BASE + bodySprite;
        model.carSet = carSet;
        model.sprites.body = sprites[i].body;
        model.sprites.choppedRoof = sprites[i].choppedRoof;
        model.sprites.airScoop = sprites[i].airScoop;
        model.sprites.rearBumperStripped = sprites[i].rearBumperStripped;
        model.sprites.frontBumperStripped = sprites[i].frontBumperStripped;
        model.name = metadata[i].classified.carName ? metadata[i].classified.carName : "";
        model.year = metadata[i].classified.year;
        model.offsets = metadata[i].offsets;
        // The data file has no wheel Y; the wheel cell is bottom-aligned to
        // the body sprite instead.
        model.offsets.offsetYTires = CarSetWheelY(carSet, bitmap.height);
        metadata[i].offsets.offsetYTires = model.offsets.offsetYTires;
        model.width = bitmap.width;
        model.height = bitmap.height;
        // FIXME Speed is unknown, same placeholder the stock cars use
        model.baseSpeed = 100;
        switch (specs[i].Brand()) {
            case 0:  model.vendor = CarVendorGM; break;
            case 1:  model.vendor = CarVendorFord; break;
            case 2:  model.vendor = CarVendorChrysler; break;
            default: model.vendor = CarVendorGeneric; break;
        }

        metadataByIndex[model.index] = metadata[i];
        allCarModels.insert(model);
        added++;
    }

    // Only the array: metadataByIndex still points at the classified strings.
    free(metadata);

    LOG_INFO("Added %d Street Rod SE cars", added);
    return added;
}

CarModel CarData::GetRandomModel() {
    return getRandomElement(allCarModels);
}

