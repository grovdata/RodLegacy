#define TOML_HEADER_ONLY 0

#include "log.h"
#include "cardata.h"
#include "utils.h"
#include <fstream>
#include <SDL.h>
#include "parsers/libp.h"

CarData::CarData() {
}

CarData::~CarData() {

}

void CarData::ReadEntries(std::vector<uint8_t> datafilebuf, std::string tomlfile, std::pair<int, std::vector<BitmapEntry_t>> bitmaps) {
    ReadCarsBuiltin(datafilebuf, bitmaps);
    ReadCarsFromTomlFile(tomlfile);
}

CarModel CarData::GetModel(char* carModel) {
    for (auto m: allCarModels) {
        bool equals = strcmp(m.id.c_str(), carModel);
        if (equals == 0) {
            LOG_INFO("Found model %s\n", carModel);            
            return m;
        }
    }
    // Not sure about this
    return CarModel();
}

CarModel CarData::GetModelFromIndex(int index) {
    for (auto m: allCarModels) {
        bool equals = m.index == index;
        if (equals) {
            LOG_INFO("Found model %s\n", m.name.c_str());
            // m.Log();
            return m;
        }
    }
    // Not sure about this
    LOG_WARN("No model found for id %d", index);
    return CarModel();
}

CarModel CarData::CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata) {
    CarModel m;
    // FIXME Speed is unknown

    LOG_INFO("bitmap w %d h%d", bitmap.width, bitmap.height);

    if (index < int(CAR_INDEX_COUNT)) {
        m.index = kCarIndex[index];
        LOG_INFO("Car body ID: %d", static_cast<int>(m.index));
    }
    m.carSet = kAmigaCarSet;
    m.sprites = AmigaCarSprites(m.index);
    m.baseSpeed = 100;
    m.filename = "";
    m.height = bitmap.height;
    m.width = bitmap.width;
    m.id = "";
    m.name = metadata.classified.carName;
    m.offsets = metadata.offsets;
    m.vendor = SrLibQueryGetCarVendor(m.index);
    m.year = metadata.classified.year;
    m.price = metadata.classified.price;
    return m;
}

bool CarData::CarIsValid(toml::table *table) {
    std::vector<std::string> mandatoryKeys = {"id", "index", "vendor", "name", "year", "baseSpeed", "width", "height", "rearWheelX", "rearWheelY", "gasCapX", "gasCapY", "frontWheelX", "frontWheelY", "driverX", "driverY"};

    for (const auto& key : mandatoryKeys) {
        if (table->find(key) == table->end()) {
            // Key not found
            return false;
        }
    }
    // All keys found
    return true;
}

bool CarData::MapContainsKey(toml::table *table, const char *key) {
    auto search = table->find(key);
    return search != table->end();
}

void CarData::ReadCarsBuiltin(std::vector<uint8_t> datafilebuf, std::pair<int, std::vector<BitmapEntry_t>> bitmaps) {
    int metadataCount;

    // Convert vector<unsigned char> to unsigned char*
    unsigned char* buffer = reinterpret_cast<uint8_t*>(datafilebuf.data());
    CarMetadata_t* metadata = dp.SrLibDataReadDataFromBuffer(buffer, datafilebuf.size(), &metadataCount);
    
    // Resize vector and copy array data
    carMetadata.clear();
    carMetadata.assign(metadata, metadata + metadataCount);
    
    for (int i = 0; i < metadataCount; i++) {   
        // Get the car's stock model index from kCarIndex array
        int stockIndex = kCarIndex[i];
        
        // Get the corresponding bitmap for this car model
        BitmapEntry_t& bitmap = bitmaps.second[stockIndex];
        
        LOG_INFO("Creating car %d (stock index %d) with bitmap w:%d h: %d", i, stockIndex, bitmap.width, bitmap.height);         
        
        // Create the car model using the stock index
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
        // A DOS set's body sprite numbers (39..179 for SRSE) overlap lib.p's
        // car assets (179..272), so shift each set clear of the last to keep
        // CarModel::index a unique pool key. The base is the same +1000
        // HOT_DATA itself uses, stepped once per set.
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
        // The data file has no wheel Y, so it comes from the measured table.
        model.offsets.offsetYTires = CarSetWheelY(carSet, bodySprite);
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

    // Only the array itself. The classified strings are malloc'd separately
    // and the shallow copies in metadataByIndex still point at them, exactly
    // as ReadCarsBuiltin leaves the stock cars' strings alive for the run.
    free(metadata);

    LOG_INFO("Added %d Street Rod SE cars", added);
    return added;
}

void CarData::ReadCarsFromTomlFile(std::string filename) {
    
    std::ifstream file(filename);
    if (!file.good()) {
        LOG_INFO("Custom cars TOML file %s does not exist, skipping", filename.c_str());
        return;
    }
    file.close();

    try {
        const auto data = toml::parse(filename);
        const auto table = toml::find<std::vector<toml::table>>(data, "cars");
        for (auto car : table) {
            if (!CarIsValid(&car)) {
                if (MapContainsKey(&car, "id")) {
                    std::string id = car["id"].as_string();
                    LOG_INFO("Car with id %s is incomplete, proceeding to the next one\n", id.c_str());
                } else {
                    LOG_INFO("Car section incomplete, proceeding to the next one\n");
                }
                continue;
            }

            CarModel newModel;
            newModel.id = car["id"].as_string();
            newModel.vendor = CarVendor_t(car["vendor"].as_integer());
            newModel.year = car["year"].as_integer();
            newModel.name = car["name"].as_string();
            if (MapContainsKey(&car, "filename")) {
                newModel.filename = car["filename"].as_string();
            }
            newModel.baseSpeed = car["baseSpeed"].as_integer();
            newModel.width = car["width"].as_integer();
            newModel.height = car["height"].as_integer();
            newModel.offsets.offsetXRearTire = car["rearWheelX"].as_integer();
            //newModel.offsets.rearWheelY = car["rearWheelY"].as_integer();
            //newModel.offsets. = car["gasCapX"].as_integer();
            //newModel.offsets.gasCapY = car["gasCapY"].as_integer();
            newModel.offsets.offsetXAirScoop = car["hoodScoopX"].as_integer();
            newModel.offsets.offsetYAirScoop = car["hoodScoopY"].as_integer();
            newModel.offsets.offsetXFrontTire = car["frontWheelX"].as_integer();
            //newModel.offsets.frontWheelY = car["frontWheelY"].as_integer();
            newModel.offsets.offsetXDriver = car["driverX"].as_integer();
            newModel.offsets.offsetYDriver = car["driverY"].as_integer();
            newModel.index = car["index"].as_integer();
            // A TOML car names a lib.p body asset, so it follows the Amiga
            // set's conventions for where its variants live.
            newModel.carSet = kAmigaCarSet;
            newModel.sprites = AmigaCarSprites(newModel.index);
            // PrintModel(newModel);
            allCarModels.insert(newModel);
        }
    } catch (const toml::type_error &e) {
        LOG_ERROR("%s", e.what());
    }
}

void CarData::PrintModel(CarModel model) {
    LOG_INFO("Car ID: %s", model.id.c_str());
    LOG_INFO("Vendor: %d", model.vendor);
    LOG_INFO("Name: %s", model.name.c_str());
    LOG_INFO("Index: %d", model.index);
    LOG_INFO("Filename: %s", model.filename.c_str());
    LOG_INFO("Year: %d", model.year);
    LOG_INFO("Base Speed: %d", model.baseSpeed);
    LOG_INFO("Width: %d, Height: %d", model.width, model.height);
};

CarModel CarData::GetRandomModel() {
    return getRandomElement(allCarModels);
}

