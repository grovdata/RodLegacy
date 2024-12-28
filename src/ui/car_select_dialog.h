#pragma once
#include "dialog.h"
#include "../car.h"

class CarSelectDialog : public Dialog {
private:
    std::function<void(Car*)> onCarSelected;
    
public:
    CarSelectDialog(std::function<void(Car*)> onCarSelected);
    void PopulateCarList();
}; 