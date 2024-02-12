#define TOML_HEADER_ONLY 0

#include "cardata.h"
#include "utils.h"
#include "include/toml.hpp"
#include <iostream>
#include <fstream>

CarData::CarData() {

}

CarData::~CarData() {

}

CarModel CarData::GetModel(CarModelId carModel) {
    for (auto m: allCarModels) {
        printf("Model %s\n", m.name.c_str());
        if (m.id == carModel) {
            printf("found model %d\n",m.id);
            return m;
        }
    }
    // Not sure about this
    return CarModel();
}

void CarData::ReadCarsFromFile(char *filename) {
    const auto data = toml::parse(filename);
    const auto table = toml::find<std::vector<toml::table>>(data, "cars");
    for (auto car : table) {
        CarModel newModel;
        // newModel.id = CAR_MODEL_GM_CHEVROLET_2DR;
        newModel.vendor = CarVendor(car["year"].as_integer());
        newModel.year = car["year"].as_integer();
        newModel.name = car["name"].as_string();
        newModel.filename = car["filename"].as_string();
        newModel.baseSpeed = car["baseSpeed"].as_integer();
        newModel.width = car["width"].as_integer();
        newModel.height = car["height"].as_integer();
        newModel.rearWheelX = car["rearWheelX"].as_integer();
        newModel.rearWheelY = car["rearWheelY"].as_integer();
        newModel.gasCapX = car["gasCapX"].as_integer();
        newModel.gasCapY = car["gasCapY"].as_integer();
        newModel.frontWheelX = car["frontWheelX"].as_integer();
        newModel.frontWheelY = car["frontWheelY"].as_integer();
        newModel.driverX = car["driverX"].as_integer();
        newModel.driverY = car["driverY"].as_integer();
        PrintModel(newModel);
        allCarModels.insert(newModel);
    }
}

void CarData::PrintModel(CarModel model) {
    printf("Car ID: %d\n", model.id);
    printf("Vendor: %d\n", model.vendor);
    printf("Name: %s\n", model.name.c_str());
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