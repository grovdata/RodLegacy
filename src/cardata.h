#pragma once
#include <set>
#include "car.h"
#include "include/toml.hpp"

class CarData {
    private:
        std::set<CarModel> allCarModels;

        bool CarIsValid(toml::table *table);
        bool MapContainsKey(toml::table *table, const char *key);

    public:
        CarData();
        ~CarData();

        /// @brief Finds a model in car data array
        /// @param modelId
        /// @return a car model
        CarModel GetModel(char* modelId);

        void PrintModel(CarModel model);
        void ReadCarsFromFile(char *filename);
        CarModel GetRandomModel();

};

