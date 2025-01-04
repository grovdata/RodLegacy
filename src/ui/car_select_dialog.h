#pragma once
#include "dialog.h"
#include "../car.h"

class CarSelectDialog : public Dialog {
private:
    std::function<void(int)> onCarSelected;
    std::vector<std::string> items;
    int selectedIndex;
    
public:
    CarSelectDialog(std::function<void(int)> onCarSelected);
    void PopulateCarList();
    void OnItemSelected(int index) override;
}; 