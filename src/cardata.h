#pragma once
#include <set>
#include "car.h"
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

        /// @brief Adds the 25 Street Rod SE data-disk cars to the pool.
        /// Everything comes out of the disk's own HOT_DATA - stats, sprite
        /// numbers and pixel offsets - so nothing here is hand-tuned.
        /// @param hotData raw SRSE HOT_DATA bytes
        /// @param bitmaps entries parsed from SRSE LIB2, for sprite sizes
        /// @return number of models added
        int ReadCarsSrse(const std::vector<uint8_t>& hotData,
                         const std::pair<int, std::vector<DOSBitmapEntry_t>>& bitmaps);

        /// @brief Finds a model in car data array
        /// @param modelId
        /// @return a car model
        CarModel GetModel(char* modelId);
        CarModel GetModelFromIndex(int index);

        CarModel CreateModel(int index, BitmapEntry_t bitmap, CarMetadata_t metadata);

        void PrintModel(CarModel model);

        void ReadCarsBuiltin(std::vector<unsigned char>, std::pair<int, std::vector<BitmapEntry_t>> bitmaps);
        void ReadCarsFromTomlFile(std::string filename);
        CarModel GetRandomModel();
};

