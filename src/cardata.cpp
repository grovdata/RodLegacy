#define TOML_HEADER_ONLY 0

#include "cardata.h"
#include "utils.h"
#include <iostream>
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
            SDL_Log("Found model %s\n", carModel);            
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
            SDL_Log("Found model %s\n", m.name.c_str());
            // m.Log();
            return m;
        }
    }
    // Not sure about this
    std::cout << "No model found for id " << index << std::endl;
    return CarModel();
}

CarModel CarData::CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata) {
    CarModel m;
    // FIXME Speed is unknown

    std::cout << "bitmap w " << bitmap.width << " h" << bitmap.height << std::endl;

    if (index <= int(CAR_INDEX_COUNT)) {
        m.index = CAR_INDEX[index];
        std::cout << "Car body ID: " << static_cast<int>(m.index) << std::endl;
    }
    m.baseSpeed = 100;
    m.filename = "";
    m.height = bitmap.height;
    m.width = bitmap.width;
    m.id = "";
    m.name = metadata.classified.carName;
    m.offsets = metadata.offsets;
    m.vendor = SrLibQueryGetCarVendor(index);
    m.year = metadata.classified.year;
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
        // Get the car's stock model index from CAR_INDEX array
        int stockIndex = CAR_INDEX[i];
        
        // Get the corresponding bitmap for this car model
        BitmapEntry_t& bitmap = bitmaps.second[stockIndex];
        
        std::cout << "Creating car " << i << " (stock index " << stockIndex << ") with bitmap w:" 
                  << bitmap.width << " h: " << bitmap.height << std::endl;         
        
        // Create the car model using the stock index
        CarModel newModel = CreateModel(i, bitmap, carMetadata[i]);
        allCarModels.insert(newModel);
    }

    std::cout << "Created " << allCarModels.size() << " car models:" << std::endl;
    for (const auto& model : allCarModels) {                
        model.Log();
    }
}

CarMetadata_t CarData::GetCarMetadata(int carIndex) {
    int index = dp.GetCarIndex((AmigaLibAsset)carIndex);
    return carMetadata[index];
}

void CarData::ReadCarsFromTomlFile(std::string filename) {
    
    std::ifstream file(filename);
    if (!file.good()) {
        SDL_Log("Custom cars TOML file %s does not exist, skipping", filename.c_str());
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
                    SDL_Log("Car with id %s is incomplete, proceeding to the next one\n", id.c_str());
                } else {
                    SDL_Log("Car section incomplete, proceeding to the next one\n");
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
            // PrintModel(newModel);
            allCarModels.insert(newModel);
        }
    } catch (const toml::type_error &e) {
        std::cout << e.what() << std::endl;
    }
}

void CarData::PrintModel(CarModel model) {
    printf("Car ID: %s\n", model.id.c_str());
    printf("Vendor: %d\n", model.vendor);
    printf("Name: %s\n", model.name.c_str());
    printf("Index: %d\n", model.index);
    printf("Filename: %s\n", model.filename.c_str());
    printf("Year: %d\n", model.year);
    printf("Base Speed: %d\n", model.baseSpeed);
    printf("Width: %d, Height: %d\n", model.width, model.height);
    //printf("Driver Position: (%d, %d)\n", model.driverX, model.driverY);
    //printf("Rear Wheel Position: (%d, %d)\n", model.rearWheelX, model.rearWheelY);
    //printf("Front Wheel Position: (%d, %d)\n", model.frontWheelX, model.frontWheelY);
    //printf("Gas Cap Position: (%d, %d)\n", model.gasCapX, model.gasCapY);
};

CarModel CarData::GetRandomModel() {
    return getRandomElement(allCarModels);
}

