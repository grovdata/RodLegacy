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

class CarData {
    private:
        std::set<CarModel>         allCarModels;
        // Metadata by CarModel::index, so lookups work for both the Amiga
        // stock cars and the Street Rod SE ones.
        std::map<int, CarMetadata_t> metadataByIndex;

        DataParser dp;

    public:
        CarData();
        ~CarData();

        CarMetadata_t GetCarMetadata(int carIndex);
        
        // Adds an Amiga game's cars: lib.p art, stats and offsets from its SR_data / SR2_data.
        // Returns the number of models added.
        int ReadCarSetAmiga(int carSet, const std::vector<uint8_t>& srData,
                            const std::pair<int, std::vector<BitmapEntry_t>>& bitmaps);

        // Adds an optional DOS car set's cars. Returns the number of models added.
        int ReadCarSet(int carSet, const std::vector<uint8_t>& hotData,
                       const std::pair<int, std::vector<DOSBitmapEntry_t>>& bitmaps);

        CarModel GetModelFromIndex(int index);

        CarModel GetRandomModel();

        const std::set<CarModel>& GetAllModels() const { return allCarModels; }
};

