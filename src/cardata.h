#pragma once
#include <set>
#include "car.h"

class CarData {
    private:
        std::set<CarModel> allCarModels;

    public:
        CarData();
        ~CarData();

        /// @brief Finds a model in car data array
        /// @param modelId
        /// @return a car model
        CarModel GetModel(CarModelId modelId);

        void PrintModel(CarModel model);
        void ReadCarsFromFile(char *filename);
        CarModel GetRandomModel();

};

