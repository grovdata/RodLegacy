#pragma once
#include <vector>
#include "car.h"

struct CarModel;

class CarData {
    private:
        vector<CarModel> allCarModels;

    public:
        CarData();
        ~CarData();

};

