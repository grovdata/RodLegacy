#pragma once
#include <set>
#include "car.h"

class CarData {
    private:
        std::set<CarModel> allCarModels;

    public:
        CarData();
        ~CarData();

        CarModel GetModel(CarModelId modelId);
        CarModel GetRandomModel();

};

