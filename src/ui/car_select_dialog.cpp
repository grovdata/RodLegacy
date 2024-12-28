#include "car_select_dialog.h"
#include "../state.h"

CarSelectDialog::CarSelectDialog(std::function<void(Car*)> onCarSelected)
    : Dialog("Your cars", {60, 40, 200, 120}, state.assets.GetMainFont()),
      onCarSelected(onCarSelected)
{
    EnableList();
    
    Button* switchItButton = new Button(
        "Switch it",
        SDL_Rect{bounds.x + 5, bounds.y + bounds.h - 25, 60, 18},
        [this]() {
            if (selectedIndex >= 0) {
                // CarModel model = state.GetCarModels()[selectedIndex];
                // Car* car = new Car(model);
                // onCarSelected(car);
                Hide();
            }
        }
    );

    Button* sellItButton = new Button(
        "Sell it",
        SDL_Rect{bounds.x + 65, bounds.y + bounds.h - 25, 60, 18},
        [this]() {
            // TODO: Implement sell logic
        }
    );
    
    Button* cancelBtn = new Button(
        "Forget it",
        SDL_Rect{bounds.x + 125, bounds.y + bounds.h - 25, 60, 18},
        [this]() { Hide(); }
    );
    
    AddButton(switchItButton);
    AddButton(sellItButton);
    AddButton(cancelBtn);
    
    // Create scroll buttons
    scrollUpBtn = new Button(
        "U",
        SDL_Rect{bounds.x + bounds.w - 20, bounds.y + 5, 15, 15},
        [this]() {
            if (scrollOffset > 0) scrollOffset--;
        }
    );
    
    scrollDownBtn = new Button(
        "D", 
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
    // for (const auto& model : state.GetCarModels()) {
    //     carNames.push_back(model.name);
    // }
    SetItems(carNames);
} 