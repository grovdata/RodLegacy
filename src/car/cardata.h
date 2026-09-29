/*
 * RodLegacy - Car model data: CarData reads the stock cars from SR_data and the data-disk sets into
 * CarModels.
 *
 * Created: 2024-02-03
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include <set>
#include "car/car.h"
#include "parsers/libp.h"
#include "include/toml.hpp"

class CarData {
    private:
        std::set<CarModel>         allCarModels;
        std::vector<CarMetadata_t> carMetadata;
        // Metadata by CarModel::index, so lookups work for both the Amiga
        // stock cars and the Street Rod SE ones.
        std::map<int, CarMetadata_t> metadataByIndex;

        bool CarIsValid(toml::table *table);
        bool MapContainsKey(toml::table *table, const char *key);

        DataParser dp;
        std::string datafile;
        std::string tomlfile;

    public:
        CarData();
        ~CarData();

        CarMetadata_t GetCarMetadata(int carIndex);
        
        void ReadEntries(std::vector<uint8_t> datafilebuf, std::string tomlfile, std::pair<int, std::vector<BitmapEntry_t>> bitmaps);

        // Adds an optional DOS car set's cars. Returns the number of models added.
        int ReadCarSet(int carSet, const std::vector<uint8_t>& hotData,
                       const std::pair<int, std::vector<DOSBitmapEntry_t>>& bitmaps);

        CarModel GetModel(char* modelId);
        CarModel GetModelFromIndex(int index);

        CarModel CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata);

        void PrintModel(CarModel model);

        void ReadCarsBuiltin(std::vector<unsigned char>, std::pair<int, std::vector<BitmapEntry_t>> bitmaps);
        void ReadCarsFromTomlFile(std::string filename);
        CarModel GetRandomModel();

        const std::set<CarModel>& GetAllModels() const { return allCarModels; }
};

