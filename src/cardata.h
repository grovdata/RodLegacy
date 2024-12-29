#pragma once
#include <set>
#include "car.h"
#include "parsers/libp.h"
#include "include/toml.hpp"

class CarData {
    private:
        std::set<CarModel>         allCarModels;
        std::vector<CarMetadata_t> carMetadata;

        bool CarIsValid(toml::table *table);
        bool MapContainsKey(toml::table *table, const char *key);

        DataParser dp;
        string datafile;
        string tomlfile;

    public:
        CarData();
        ~CarData();

        CarMetadata_t GetCarMetadata(int carIndex);
        void ReadEntries(string datafile, string tomlfile, pair<int, vector<BitmapEntry_t>> bitmaps);

        /// @brief Finds a model in car data array
        /// @param modelId
        /// @return a car model
        CarModel GetModel(char* modelId);
        CarModel GetModelFromIndex(int index);

        CarModel CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata);

        void PrintModel(CarModel model);

        void ReadCarsBuiltin(string datafile, pair<int, vector<BitmapEntry_t>> bitmaps);
        void ReadCarsFromTomlFile(string filename);
        CarModel GetRandomModel();
};

