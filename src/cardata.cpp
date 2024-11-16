#define TOML_HEADER_ONLY 0

#include "cardata.h"
#include "utils.h"
#include <iostream>
#include <fstream>

using namespace std;

CarData::CarData() {

}

CarData::~CarData() {

}

CarModel CarData::GetModel(char* carModel) {
    for (auto m: allCarModels) {
        bool equals = strcmp(m.id.c_str(), carModel);
        if (equals == 0) {
            // printf("Found model %s\n", carModel);
            return m;
        }
    }
    // Not sure about this
    return CarModel();
}

bool CarData::CarIsValid(toml::table *table) {
    vector<string> mandatoryKeys = {"id", "index", "vendor", "name", "year", "baseSpeed", "width", "height", "rearWheelX", "rearWheelY", "gasCapX", "gasCapY", "frontWheelX", "frontWheelY", "driverX", "driverY"};

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

void CarData::ReadCarsFromTomlFile(char *filename) {
    try {
        const auto data = toml::parse(filename);
        const auto table = toml::find<vector<toml::table>>(data, "cars");
        for (auto car : table) {
            if (!CarIsValid(&car)) {
                if (MapContainsKey(&car, "id")) {
                    string id = car["id"].as_string();
                    printf("Car with id %s is incomplete, proceeding to the next one\n", id.c_str());
                } else {
                    printf("Car section incomplete, proceeding to the next one\n");
                }
                continue;
            }

            CarModel newModel;
            newModel.id = car["id"].as_string();
            newModel.vendor = CarVendor(car["vendor"].as_integer());
            newModel.year = car["year"].as_integer();
            newModel.name = car["name"].as_string();
            if (MapContainsKey(&car, "filename")) {
                newModel.filename = car["filename"].as_string();
            }
            newModel.baseSpeed = car["baseSpeed"].as_integer();
            newModel.width = car["width"].as_integer();
            newModel.height = car["height"].as_integer();
            newModel.rearWheelX = car["rearWheelX"].as_integer();
            newModel.rearWheelY = car["rearWheelY"].as_integer();
            newModel.gasCapX = car["gasCapX"].as_integer();
            newModel.gasCapY = car["gasCapY"].as_integer();
            newModel.hoodScoopX = car["hoodScoopX"].as_integer();
            newModel.hoodScoopY = car["hoodScoopY"].as_integer();
            newModel.frontWheelX = car["frontWheelX"].as_integer();
            newModel.frontWheelY = car["frontWheelY"].as_integer();
            newModel.driverX = car["driverX"].as_integer();
            newModel.driverY = car["driverY"].as_integer();
            newModel.index = car["index"].as_integer();
            // PrintModel(newModel);
            allCarModels.insert(newModel);
        }
    } catch (const toml::type_error &e) {
        cout << e.what() << endl;
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
    printf("Driver Position: (%d, %d)\n", model.driverX, model.driverY);
    printf("Rear Wheel Position: (%d, %d)\n", model.rearWheelX, model.rearWheelY);
    printf("Front Wheel Position: (%d, %d)\n", model.frontWheelX, model.frontWheelY);
    printf("Gas Cap Position: (%d, %d)\n", model.gasCapX, model.gasCapY);
};

CarModel CarData::GetRandomModel() {
    return getRandomElement(allCarModels);
}