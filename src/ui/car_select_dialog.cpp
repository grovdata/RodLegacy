#include "car_select_dialog.h"
#include "../state.h"

CarSelectDialog::CarSelectDialog(std::function<void(Car*)> onCarSelected)
    : Dialog("Your cars", {36, 26, 246, 136}, state.assets.GetMainFont()),
      onCarSelected(onCarSelected)
{
    listBoxRect = {0, 0, 242, 80};
    EnableList();
    
    Button* switchItButton = new Button(
        "Switch it",
        SDL_Rect{bounds.x + 14, bounds.y + bounds.h - 26, 66, 16},
        [this]() {
            if (selectedIndex >= 0) {
                CarModel model = state.GetOwnedCars()[selectedIndex]->GetModel();
                Car* car = new Car(model);
                this->onCarSelected(car);
                Hide();
            }
        }
    );

    Button* sellItButton = new Button(
        "Sell it",
        SDL_Rect{bounds.x + 65, bounds.y + bounds.h - 26, 66, 16},
        [this]() {
            // TODO: Implement sell logic
        }
    );
    
    Button* cancelBtn = new Button(
        "Forget it",
        SDL_Rect{bounds.x + 125, bounds.y + bounds.h - 26, 66, 16},
        [this]() { Hide(); }
    );
    
    AddButton(switchItButton);
    AddButton(sellItButton);
    AddButton(cancelBtn);
    
    // Create scroll buttons
    scrollUpBtn = new Button(
        "/",
        SDL_Rect{bounds.x + bounds.w - 20, bounds.y + 5, 15, 15},
        [this]() {
            if (scrollOffset > 0) scrollOffset--;
        }
    );
    
    scrollDownBtn = new Button(
        ">", 
        SDL_Rect{bounds.x + bounds.w - 20, bounds.y + bounds.h - 40, 15, 15},
        [this]() {
            if (scrollOffset + visibleItems < static_cast<int>(items.size())) scrollOffset++;
        }
    );
    
    AddButton(scrollUpBtn);
    AddButton(scrollDownBtn);
    
    PopulateCarList();
}

void CarSelectDialog::PopulateCarList() {
    std::vector<std::string> carNames;
    for (const auto& model : state.GetOwnedCars()) {
        carNames.push_back(model->GetModel().name);
    }
    SetItems(carNames);
} 