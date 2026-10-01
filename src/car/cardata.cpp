#include "core/log.h"
#include "car/cardata.h"
#include "core/utils.h"
#include <fstream>
#include <SDL.h>
#include "parsers/libp.h"

CarData::CarData() {
}

CarData::~CarData() {

}

void CarData::ReadEntries(std::vector<uint8_t> datafilebuf, std::pair<int, std::vector<BitmapEntry_t>> bitmaps) {
    ReadCarsBuiltin(datafilebuf, bitmaps);
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

CarModel CarData::CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata) {
    CarModel m;

    LOG_INFO("bitmap w %d h%d", bitmap.width, bitmap.height);

    if (index < int(CAR_INDEX_COUNT)) {
        m.index = kCarIndex[index];
        LOG_INFO("Car body ID: %d", static_cast<int>(m.index));
    }
    m.carSet = kAmigaCarSet;
    m.sprites = AmigaCarSprites(m.index);
    m.sprites.airScoop = metadata.airScoop;
    m.baseSpeed = 100;
    m.height = bitmap.height;
    m.width = bitmap.width;
    m.name = metadata.classified.carName;
    m.offsets = metadata.offsets;
    m.vendor = SrLibQueryGetCarVendor(m.index);
    m.year = metadata.classified.year;
    m.price = metadata.classified.price;
    return m;
}

void CarData::ReadCarsBuiltin(std::vector<uint8_t> datafilebuf, std::pair<int, std::vector<BitmapEntry_t>> bitmaps) {
    int metadataCount;

    unsigned char* buffer = reinterpret_cast<uint8_t*>(datafilebuf.data());
    CarMetadata_t* metadata = dp.SrLibDataReadDataFromBuffer(buffer, datafilebuf.size(), &metadataCount);
    
    carMetadata.clear();
    carMetadata.assign(metadata, metadata + metadataCount);
    
    for (int i = 0; i < metadataCount; i++) {   
        int stockIndex = kCarIndex[i];
        
        BitmapEntry_t& bitmap = bitmaps.second[stockIndex];
        
        LOG_INFO("Creating car %d (stock index %d) with bitmap w:%d h: %d", i, stockIndex, bitmap.width, bitmap.height);         
        
        // SR_data has no wheel Y; the wheel cell is bottom-aligned to the body
        // sprite, which only the caller knows the height of.
        carMetadata[i].offsets.offsetYTires = CarSetWheelY(kAmigaCarSet, bitmap.height);

        CarModel newModel = CreateModel(i, bitmap, carMetadata[i]);
        metadataByIndex[newModel.index] = carMetadata[i];
        allCarModels.insert(newModel);
    }

    LOG_INFO("Created %zu car models:", allCarModels.size());
    for (const auto& model : allCarModels) {                
        model.Log();
    }
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
    const std::vector<DosCarSprites> sprites = dp.ParseCarSpritesDOS(hotData);
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
            default: model.vendor = CarVendorUnknown; break;
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

